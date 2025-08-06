struct Complex {
    float real;
    float imag;
};

Complex Complex_add(Complex a, Complex b) {
    return Complex(a.real + b.real, a.imag + b.imag);
}

Complex Complex_add(float a, Complex b) {
    return Complex(a + b.real, b.imag);
}

Complex Complex_subtract(Complex a, Complex b) {
    return Complex(a.real - b.real, a.imag - b.imag);
}

Complex Complex_subtract(float a, Complex b) {
    return Complex(a - b.real, -b.imag);
}

Complex Complex_multiply(Complex a, float b) {
    Complex c;
    c.real = b * a.real;
    c.imag = b * a.imag;
    return c;
}

Complex Complex_multiply(Complex a, Complex b) {
    Complex c;
    c.real = a.real * b.real - a.imag * b.imag;
    c.imag = a.real * b.imag + a.imag * b.real;
    return c;
}

Complex Complex_sqr(Complex c) {
    return Complex_multiply(c, c);
}

Complex Complex_divide(Complex a, Complex b) {
    float scale = 1.0 / (b.real * b.real + b.imag * b.imag);
    Complex c;
    c.real = scale * (a.real * b.real + a.imag * b.imag);
    c.imag = scale * (a.imag * b.real - a.real * b.imag);
    return c;
}

Complex Complex_divide(float a, Complex b) {
    return Complex_divide(Complex(a, 0.0), b);
}

float Complex_norm(Complex c) {
    return c.real * c.real + c.imag * c.imag;
}

float Complex_abs(Complex c) {
    return sqrt(Complex_norm(c));
}

Complex Complex_sqrt(Complex c) {
    float n = Complex_abs(c);
    float t1 = sqrt(0.5) * (n + abs(c.real));
    float t2 = 0.5 * c.imag / t1;

    if (n == 0) return Complex(0.0, 0.0);
    if (c.real >= 0) return Complex(t1, t2);
    else return Complex(abs(t2), t1 * sign(c.imag)); // TODO this differs from std::copysign when c.imag == 0
}