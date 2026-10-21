#!/usr/bin/env python3
"""
gen_float_const_cases.py - float and double instructions with constant operands

Adds single-instruction cases to test/nd500-conformance.json for:

  * short constants ($0..$63, the 6-bit form, codes 00H..3FH) as operands of F and D
    instructions, ND-05.009.4 Table 13 (CF / CDF): the hardware converts the constant to
    float or double float before the instruction uses it;
  * byte, halfword, word/float and double constants, the same table:
      F:  :B :H -> CF      :W :F -> NC      :D -> IOS (illegal operand specifier)
      D:  :B :H -> CDF     :W :F -> 32LZ    :D -> NC
  * floating divide by zero (6.5.1, DZ): the destination gets the largest possible value
    with the sign of the dividend; zero divided by zero gives zero.

The expected values come from the reference arithmetic below, written from the manual:
7.2.5 / 7.2.6 (format: sign, 9-bit exponent with bias 256, 22 or 54 stored mantissa bits
plus the implicit bit, exponent 0 = exactly zero) and 7.2.7 (rounding: add one when
G=1 and (St=1 or L=1)). It uses exact fractions. It never runs the emulator.

Usage:  python3 tools/gen_float_const_cases.py [path-to-nd500-conformance.json]
The script removes every case whose name starts with "FConst_" and appends the fresh
ones, so running it twice gives the same file.
"""
import json
import sys
from fractions import Fraction

PREFIX = "FConst_"

PC0 = 4096
B0 = 65536            # the B register the corpus uses for local operands
POISON = [0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF]

Z, S, DZ = 0x20, 0x80, 0x1000   # ST1 bits 5, 7, 12 (manual 6.5.1)


# ---------------------------------------------------------------- reference arithmetic

def mant_bits(dbl):
    return 54 if dbl else 22


def decode(bits, dbl):
    """Value of a float (dbl False) or double (dbl True) bit pattern, manual 7.2.5/7.2.6."""
    mb = mant_bits(dbl)
    sign = (bits >> (mb + 9)) & 1
    e = (bits >> mb) & 0x1FF
    if e == 0:
        return Fraction(0)                       # exactly zero, whatever the rest says
    m = Fraction((1 << mb) | (bits & ((1 << mb) - 1)), 1 << (mb + 1))   # 0.5 <= M < 1
    v = m * Fraction(2) ** (e - 256)
    return -v if sign else v


def encode(value, dbl):
    """Bit pattern of the value, rounded once by manual 7.2.7."""
    mb = mant_bits(dbl)
    if value == 0:
        return 0
    sign = 1 if value < 0 else 0
    mag = abs(value)
    # find e with 0.5 <= mag / 2**e < 1
    e = mag.numerator.bit_length() - mag.denominator.bit_length()
    while mag / Fraction(2) ** e >= 1:
        e += 1
    while mag / Fraction(2) ** e < Fraction(1, 2):
        e -= 1
    scaled = mag / Fraction(2) ** (e - (mb + 1))        # in [2**mb, 2**(mb+1))
    q = scaled.numerator // scaled.denominator
    rest = scaled - q                                    # 0 <= rest < 1
    g = rest >= Fraction(1, 2)
    st = rest > Fraction(1, 2)
    low = q & 1                                          # L
    if g and (st or low):
        q += 1
        if q == 1 << (mb + 1):
            q >>= 1
            e += 1
    field = e + 256
    assert 1 <= field <= 511, "reference value out of range: %s" % value
    return (sign << (mb + 9)) | (field << mb) | (q & ((1 << mb) - 1))


def largest(sign, dbl):
    """Largest possible value with a sign: exponent field 511, mantissa all ones."""
    mb = mant_bits(dbl)
    return (sign << (mb + 9)) | (511 << mb) | ((1 << mb) - 1)


def is_zero(bits, dbl):
    return ((bits >> mant_bits(dbl)) & 0x1FF) == 0


def sign_of(bits, dbl):
    return (bits >> (mant_bits(dbl) + 9)) & 1


def arith(op, a_bits, b_bits, dbl):
    """a op b. Returns (result bits, dz). Divide by zero: manual 6.5.1, DZ."""
    a = decode(a_bits, dbl)
    b = decode(b_bits, dbl)
    if op == "+":
        return encode(a + b, dbl), False
    if op == "-":
        return encode(a - b, dbl), False
    if op == "*":
        return encode(a * b, dbl), False
    assert op == "/"
    if b == 0:
        if a == 0:
            return 0, True                               # zero divided by zero gives zero
        return largest(sign_of(a_bits, dbl), dbl), True  # largest value, sign of the dividend
    return encode(a / b, dbl), False


def flags_zs(bits, dbl):
    return (Z if is_zero(bits, dbl) else 0) | (S if sign_of(bits, dbl) else 0)


# ---------------------------------------------------------------- constants (Table 13)

def short_value(code):
    """6-bit constant, two's complement: codes 20H..3FH are -32..-1 (Table 13 'SX')."""
    return code - 64 if code & 0x20 else code


class Const:
    """An operand constant: its operand-specifier bytes and the bit pattern the
    instruction type sees after the Table 13 conversion."""
    def __init__(self, tag, spec, bits_for, text=None):
        self.tag = tag
        self.text = text if text is not None else tag     # operand as written in assembly
        self.spec = spec            # operand specifier bytes
        self.bits_for = bits_for    # dbl -> bit pattern the instruction uses


def c_short(code):
    v = short_value(code)
    return Const("S%02d" % code, [code], lambda dbl: encode(Fraction(v), dbl),
                 "%d:S" % v)     # CF / CDF


def c_byte(v):
    return Const("B%+d" % v, [0xCD, v & 0xFF], lambda dbl: encode(Fraction(v), dbl),
                 "%d:B" % v)


def c_half(v):
    u = v & 0xFFFF
    return Const("H%+d" % v, [0xCE, u >> 8, u & 0xFF], lambda dbl: encode(Fraction(v), dbl),
                 "%d:H" % v)


def c_word(bits32):
    # :W and :F share the code 0CFH. F: no conversion. D: 32 least significant bits zero filled.
    spec = [0xCF] + [(bits32 >> s) & 0xFF for s in (24, 16, 8, 0)]
    return Const("W%08X" % bits32, spec, lambda dbl: (bits32 << 32) if dbl else bits32,
                 "0x%08X:W" % bits32)


def c_dbl(bits64):
    spec = [0xCC] + [(bits64 >> s) & 0xFF for s in range(56, -8, -8)]
    return Const("D%016X" % bits64, spec, lambda dbl: bits64, "0x%016X:D" % bits64)


# ---------------------------------------------------------------- case building

def be(bits, nbytes):
    return [(bits >> (8 * (nbytes - 1 - i))) & 0xFF for i in range(nbytes)]


def ram_bytes(addr, data):
    return [[addr + i, v] for i, v in enumerate(data)]


def regs_f(bits, dbl, base="a1", low="e1"):
    """Register file entries holding a float (A) or double (A high, E low)."""
    if dbl:
        return {base: bits >> 32, low: bits & 0xFFFFFFFF}
    return {base: bits}


def case(name, assembly, nbytes, initial_regs, initial_ram, final_regs, final_ram, trap=None,
         advance=True):
    initial_regs = dict(initial_regs)
    initial_regs.update({"pc": PC0, "b": B0, "st": 0})
    final_regs = dict(final_regs)
    if advance:
        final_regs["pc"] = PC0 + len(nbytes)
    c = {
        "name": PREFIX + name,
        "assembly": assembly,
        "bytes": nbytes,
        "initial": {"regs": initial_regs, "ram": initial_ram},
        "final": {"regs": final_regs, "ram": final_ram},
        "maxInstructions": 1,
    }
    if trap:
        c["expectedTrap"] = trap[0]
        c["expectedTrapBits"] = trap[1]
    return c


DZ_TRAP = ("DivisionByZero", DZ)

REG_OP = {  # F, D opcodes of Fn/Dn op <operand>, n = 1 (manual 11.1 to 11.4, 10.9)
    "+": (0x58, 0x5C), "-": (0x64, 0x68), "*": (0x70, 0x74), "/": (0x7C, 0xE8),
}
COMP = (0x38, 0x3C)
LOAD = (0x10, 0x14)
MOVE = (0x1B, 0x2C)
OP2 = {"+": (0x56, 0x57), "-": (0x5B, 0x5C), "*": (0x60, 0x61), "/": (0x65, 0x66)}   # after FCH
OP3 = {"+": (0x6A, 0x6B), "-": (0x6F, 0x70), "*": (0x74, 0x75), "/": (0x79, 0x7A)}   # after FCH
COMP2 = (0x2F, 0x40)
NAME2 = {"+": "ADD2", "-": "SUB2", "*": "MUL2", "/": "DIV2"}
NAME3 = {"+": "ADD3", "-": "SUB3", "*": "MUL3", "/": "DIV3"}
NAMEO = {"+": "add", "-": "sub", "*": "mul", "/": "div"}
LOC0, LOC8 = 0x40, 0x42      # short local operands b.0 and b.8 (displacement x 4)


def xs_for(dbl):
    """Values the other operand takes: positive, negative, zero, a long mantissa."""
    vals = [Fraction(3), Fraction(-5, 2), Fraction(0), Fraction(1, 3)]
    if dbl:
        vals.append(Fraction(123456789123456789, 1000000000))
    return vals


def cases_for_const(cn, dbl, ops_full=True, xs=None):
    """Every instruction form that takes the constant as a read operand."""
    T = "D" if dbl else "F"
    nb = 8 if dbl else 4
    cbits = cn.bits_for(dbl)
    out = []
    tag = "%s_%s" % (T, cn.tag)
    ci = 1 if dbl else 0
    xs = xs if xs is not None else xs_for(dbl)

    # MOVE const,b.0
    data = be(cbits, nb)
    out.append(case("%s_MOVE" % tag, "%s MOVE $%s,b.0" % (T, cn.text), [MOVE[ci]] + cn.spec + [LOC0],
                    {}, ram_bytes(B0, POISON), {"st": flags_zs(cbits, dbl)}, ram_bytes(B0, data)))
    # Fn := const  /  Dn := const
    out.append(case("%s_LOAD" % tag, "%s1 := $%s" % (T, cn.text), [LOAD[ci]] + cn.spec,
                    {"a1": 0xDEADBEEF, "e1": 0xCAFEF00D} if dbl else {"a1": 0xDEADBEEF},
                    [], dict(regs_f(cbits, dbl), st=flags_zs(cbits, dbl)), []))
    if not ops_full:
        return out
    for xv in xs:
        xb = encode(xv, dbl)
        xtag = "x%s" % ("%X" % xb)
        # Fn op const
        for op in "+-*/":
            r, dz = arith(op, xb, cbits, dbl)
            st = flags_zs(r, dbl) | (DZ if dz else 0)
            out.append(case("%s_REG%s_%s" % (tag, NAMEO[op], xtag), "%s1 %s $%s" % (T, op, cn.text),
                            [REG_OP[op][ci]] + cn.spec, regs_f(xb, dbl), [],
                            dict(regs_f(r, dbl), st=st), [], DZ_TRAP if dz else None))
        # Fn COMP const  (a - c, Z and S from the exact difference; C and O clear)
        d = decode(xb, dbl) - decode(cbits, dbl)
        st = (Z if d == 0 else 0) | (S if d < 0 else 0)
        out.append(case("%s_COMP_%s" % (tag, xtag), "%s1 COMP $%s" % (T, cn.text), [COMP[ci]] + cn.spec,
                        regs_f(xb, dbl), [], {"st": st}, []))
        # t OP2 b.0,const   (<a> op <b> -> <a>)
        for op in "+-*/":
            r, dz = arith(op, xb, cbits, dbl)
            st = flags_zs(r, dbl) | (DZ if dz else 0)
            out.append(case("%s_%s_%s" % (tag, NAME2[op], xtag), "%s %s b.0,$%s" % (T, NAME2[op], cn.text),
                            [0xFC, OP2[op][ci], LOC0] + cn.spec, {}, ram_bytes(B0, be(xb, nb)),
                            {"st": st}, ram_bytes(B0, be(r, nb)), DZ_TRAP if dz else None))
        # t OP3 b.0,const,b.8 and const,b.0,b.8   (<a> op <b> -> <c>)
        for op in "+-*/":
            r, dz = arith(op, xb, cbits, dbl)
            st = flags_zs(r, dbl) | (DZ if dz else 0)
            out.append(case("%s_%s_MC_%s" % (tag, NAME3[op], xtag),
                            "%s %s b.0,$%s,b.8" % (T, NAME3[op], cn.text),
                            [0xFC, OP3[op][ci], LOC0] + cn.spec + [LOC8], {},
                            ram_bytes(B0, be(xb, nb)) + ram_bytes(B0 + 8, POISON[:nb]),
                            {"st": st}, ram_bytes(B0 + 8, be(r, nb)), DZ_TRAP if dz else None))
            r, dz = arith(op, cbits, xb, dbl)
            st = flags_zs(r, dbl) | (DZ if dz else 0)
            out.append(case("%s_%s_CM_%s" % (tag, NAME3[op], xtag),
                            "%s %s $%s,b.0,b.8" % (T, NAME3[op], cn.text),
                            [0xFC, OP3[op][ci]] + cn.spec + [LOC0, LOC8], {},
                            ram_bytes(B0, be(xb, nb)) + ram_bytes(B0 + 8, POISON[:nb]),
                            {"st": st}, ram_bytes(B0 + 8, be(r, nb)), DZ_TRAP if dz else None))
        # t COMP2 b.0,const and const,b.0
        d = decode(xb, dbl) - decode(cbits, dbl)
        st = (Z if d == 0 else 0) | (S if d < 0 else 0)
        out.append(case("%s_COMP2_MC_%s" % (tag, xtag), "%s COMP2 b.0,$%s" % (T, cn.text),
                        [COMP2[ci], LOC0] + cn.spec, {}, ram_bytes(B0, be(xb, nb)), {"st": st}, []))
        st = (Z if d == 0 else 0) | (S if d > 0 else 0)
        out.append(case("%s_COMP2_CM_%s" % (tag, xtag), "%s COMP2 $%s,b.0" % (T, cn.text),
                        [COMP2[ci]] + cn.spec + [LOC0], {}, ram_bytes(B0, be(xb, nb)), {"st": st}, []))
    return out


def cases_divide_by_zero():
    """Divide by zero without a constant: register and memory divisors, F and D, every
    sign of the dividend, a zero dividend, and an exponent-0 divisor with a nonzero
    mantissa (an operand with exponent 0 is exactly zero, 7.2.5)."""
    out = []
    for dbl in (False, True):
        T = "D" if dbl else "F"
        nb = 8 if dbl else 4
        ci = 1 if dbl else 0
        # 0x2AAAA has a zero exponent field (bits 22 and up) in both formats and a nonzero mantissa
        divisors = [("ZERO", 0), ("EXP0MANT", 0x2AAAA)]
        for dname, dv in divisors:
            for xname, xv in (("POS", Fraction(7, 2)), ("NEG", Fraction(-7, 2)), ("ZERO", Fraction(0))):
                xb = encode(xv, dbl)
                r, dz = arith("/", xb, dv, dbl)
                assert dz
                st = flags_zs(r, dbl) | DZ
                tag = "%s_DZ_%s_%s" % (T, dname, xname)
                # register divisor: Fn / A2 would need a register operand; use memory and
                # the DIV2/DIV3 forms with both operands in memory.
                out.append(case(tag + "_DIV2", "%s DIV2 b.0,b.8" % T, [0xFC, OP2["/"][ci], LOC0, LOC8], {},
                                ram_bytes(B0, be(xb, nb)) + ram_bytes(B0 + 8, be(dv, nb)),
                                {"st": st}, ram_bytes(B0, be(r, nb)), DZ_TRAP))
                out.append(case(tag + "_DIV3", "%s DIV3 b.0,b.8,b.16" % T,
                                [0xFC, OP3["/"][ci], LOC0, LOC8, 0x44], {},
                                ram_bytes(B0, be(xb, nb)) + ram_bytes(B0 + 8, be(dv, nb))
                                + ram_bytes(B0 + 16, POISON[:nb]),
                                {"st": st}, ram_bytes(B0 + 16, be(r, nb)), DZ_TRAP))
                out.append(case(tag + "_REGDIV", "%s1 / b.8" % T, [REG_OP["/"][ci], LOC8],
                                regs_f(xb, dbl), ram_bytes(B0 + 8, be(dv, nb)),
                                dict(regs_f(r, dbl), st=st), [], DZ_TRAP))
    return out


def cases_ios():
    """F instruction with a double constant: Table 13 gives IOS for type F with :D."""
    out = []
    spec = c_dbl(encode(Fraction(3, 2), True)).spec
    for name, nbytes, regs in (
            ("MOVE", [MOVE[0]] + spec + [LOC0], {}),
            ("LOAD", [LOAD[0]] + spec, {"a1": 0}),
            ("ADD", [REG_OP["+"][0]] + spec, {"a1": encode(Fraction(1), False)})):
        out.append(case("F_IOS_%s" % name, "F %s $D" % name, nbytes, regs, [],
                        {}, [], ("IllegalOperandValue", 1 << 34), advance=False))
    return out


# ---------------------------------------------------------------- the lists of constants

F_WORDS = [encode(v, False) for v in
           (Fraction(3, 2), Fraction(-5, 2), Fraction(1, 2), Fraction(1, 4), Fraction(1),
            Fraction(1, 10), Fraction(314159, 100000), Fraction(10), Fraction(123456, 1000),
            Fraction(-1), Fraction(1, 3), Fraction(10) ** 20, Fraction(1, 10 ** 20))]
D_WORDS = [encode(v, True) for v in
           (Fraction(3, 2), Fraction(-5, 2), Fraction(1, 10), Fraction(314159, 100000),
            Fraction(1, 3), Fraction(10) ** 30, Fraction(-123456789, 1000))]
HALVES = [0, 1, -1, 2, 9, 10, 100, -100, 127, 128, -128, 255, 256, -256, 1000, 12345, -12345,
          32767, -32768, 4096, -4097, 20000]
BYTE_FULL = list(range(-128, 128))
BYTE_ARITH = [0, 1, -1, 2, 5, 10, 31, 32, -32, 63, 64, 100, 127, -128, -127, -100]


def all_cases():
    out = []
    # 1. every short constant code, every instruction form, F and D
    for dbl in (False, True):
        for code in range(64):
            out += cases_for_const(c_short(code), dbl, True, xs_for(dbl)[:3])
    # 2. byte constants: all 256 values for MOVE and :=, a spread for the arithmetic forms
    for dbl in (False, True):
        for v in BYTE_FULL:
            out += cases_for_const(c_byte(v), dbl, v in BYTE_ARITH)
    # 3. halfword constants
    for dbl in (False, True):
        for v in HALVES:
            out += cases_for_const(c_half(v), dbl, True, xs_for(dbl)[:3])
    # 4. word/float constants: F no conversion, D 32LZ
    for w in F_WORDS:
        out += cases_for_const(c_word(w), False, True, xs_for(False)[:3])
        out += cases_for_const(c_word(w), True, True, xs_for(True)[:3])
    # 5. double constants: D no conversion
    for q in D_WORDS:
        out += cases_for_const(c_dbl(q), True, True, xs_for(True)[:3])
    # 6. F with a double constant is an illegal operand specifier
    out += cases_ios()
    # 7. divide by zero, plain operands
    out += cases_divide_by_zero()
    return out


FIXUP_DZ = {   # name: (data type is double, destination register)
    "FloatExact_Div_F_30": (False, "a1"), "FloatExact_Div_D_30": (True, "a1"),
    "FloatExact_DIV2_F_30": (False, "a1"), "FloatExact_DIV2_D_30": (True, "a1"),
    "FloatExact_DIV3_F_30": (False, "a3"), "FloatExact_DIV3_D_30": (True, "a3"),
}


def fix_divide_by_zero_cases(data):
    """Six older cases (a float divide by an exponent-0 divisor) expected the destination
    unchanged and C and O left set. Manual 6.5.1: the destination gets the largest value
    with the sign of the dividend, and every data status bit the instruction does not name
    is reset (Z, S, O, FU, FO, DZ are the bits the divide names; C is not). The expected
    final state is recomputed here from the case's own initial registers."""
    n = 0
    for c in data:
        if c["name"] not in FIXUP_DZ:
            continue
        dbl, dest = FIXUP_DZ[c["name"]]
        r = c["initial"]["regs"]
        if dbl:
            a = (r["a1"] << 32) | r["e1"]
            b = (r["a2"] << 32) | r["e2"]
        else:
            a, b = r["a1"], r["a2"]
        q, dz = arith("/", a, b, dbl)
        assert dz
        fr = c["final"]["regs"]
        fr.update(regs_f(q, dbl, dest, "e" + dest[1]))
        fr["st"] = flags_zs(q, dbl) | DZ
        n += 1
    assert n == len(FIXUP_DZ)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "test/nd500-conformance.json"
    with open(path) as f:
        data = json.load(f)
    data = [c for c in data if not c["name"].startswith(PREFIX)]
    fix_divide_by_zero_cases(data)
    fresh = all_cases()
    names = set(c["name"] for c in fresh)
    assert len(names) == len(fresh), "duplicate case names"
    data += fresh
    with open(path, "w") as f:
        f.write(json.dumps(data, indent=2))
    print("added %d cases, file now has %d" % (len(fresh), len(data)))


if __name__ == "__main__":
    main()
