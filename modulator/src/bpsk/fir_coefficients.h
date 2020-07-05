// Filter defined in `make_coefficients.py` with the following parameters:
// taps: 5
// rate: 62500
// bandwidth: 18000
#define FIR_STAGES 5
static float fir_coefficients[FIR_STAGES] = {
    0.007602129069420533f,
    -0.21697500566330508f,
    0.5508457305345488f,
    -0.21697500566330508f,
    0.007602129069420533f,
};
