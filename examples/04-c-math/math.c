// K&R style add
int add(a, b)
int a;
int b;
{
    return a + b;
}

// sub function
int sub(a,b)
int a;
int b;
{
  return a-b;
}

// Main
main()
{
    int a, b, c, d;
    char buf[32];
    int i, n;
    int x,y;

    a = 12;
    b = 5;
    c = a * b + 7;
    d = c - a / 2;

    /* convert integer to string manually */
    i = 0;
    if (d == 0)
        buf[i++] = '0';
    else {
        n = d;
        if (n < 0) {
            buf[i++] = '-';
            n = -n;
        }
        /* store digits in reverse order */
        {
            char tmp[16];
            int j = 0;
            while (n > 0) {
                tmp[j++] = '0' + (n % 10);
                n /= 10;
            }
            while (j > 0)
                buf[i++] = tmp[--j];
        }
    }
    buf[i++] = '\n';

    x = add(5,7);
    y = sub(x,2);

    /* write result to stdout (file descriptor 1) */
    write(1, buf, i);
}

