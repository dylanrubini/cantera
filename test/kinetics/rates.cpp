#include "gtest/gtest.h"
#include "cantera/thermo.h"
#include "cantera/kinetics.h"
#include "cantera/thermo/IdealGasPhase.h"
#include "cantera/kinetics/GasKinetics.h"
#include "cantera/base/Solution.h"
#include "cantera/base/Interface.h"

namespace Cantera
{

class FracCoeffTest : public testing::Test
{
public:
    FracCoeffTest() :
        therm("frac.yaml", "gas")
    {
        kin = newKinetics({&therm}, "frac.yaml", "gas");
        therm.setState_TPX(2000, 4*OneAtm,
                            "H2O:0.5, OH:.05, H:0.1, O2:0.15, H2:0.2");
        kH2O = therm.speciesIndex("H2O");
        kH = therm.speciesIndex("H");
        kOH = therm.speciesIndex("OH");
        kO2 = therm.speciesIndex("O2");
        kH2 = therm.speciesIndex("H2");
    }
    IdealGasPhase therm;
    unique_ptr<Kinetics> kin;
    size_t kH2O, kH, kOH, kO2, kH2;
};

TEST_F(FracCoeffTest, StoichCoeffs)
{
    EXPECT_DOUBLE_EQ(1.0, kin->reactantStoichCoeff(kH2O, 0));
    EXPECT_DOUBLE_EQ(1.4, kin->productStoichCoeff(kH, 0));
    EXPECT_DOUBLE_EQ(0.6, kin->productStoichCoeff(kOH, 0));
    EXPECT_DOUBLE_EQ(0.2, kin->productStoichCoeff(kO2, 0));

    EXPECT_DOUBLE_EQ(0.7, kin->reactantStoichCoeff(kH2, 1));
    EXPECT_DOUBLE_EQ(0.6, kin->reactantStoichCoeff(kOH, 1));
    EXPECT_DOUBLE_EQ(0.2, kin->reactantStoichCoeff(kO2, 1));
    EXPECT_DOUBLE_EQ(1.0, kin->productStoichCoeff(kH2O, 1));
}

TEST_F(FracCoeffTest, RateConstants)
{
    vector_fp kf(kin->nReactions(), 0.0);
    vector_fp kr(kin->nReactions(), 0.0);
    kin->getFwdRateConstants(&kf[0]);
    kin->getRevRateConstants(&kr[0]);

    // sum of reaction orders is 1.0; kf has units of 1/s
    EXPECT_DOUBLE_EQ(1e13, kf[0]);

    // sum of reaction orders is 3.8.
    // kf = 1e13 (mol/cm^3)^-2.8 s^-1 = 1e13*1000^-2.8 (kmol/m^3)^-2.8 s^-1
    EXPECT_NEAR(1e13*pow(1e3, -2.8), kf[1], 1e-2);

    // Reactions are irreversible
    EXPECT_DOUBLE_EQ(0.0, kr[0]);
    EXPECT_DOUBLE_EQ(0.0, kr[1]);
}

TEST_F(FracCoeffTest, RatesOfProgress)
{
    vector_fp kf(kin->nReactions(), 0.0);
    vector_fp conc(therm.nSpecies(), 0.0);
    vector_fp ropf(kin->nReactions(), 0.0);
    therm.getConcentrations(&conc[0]);
    kin->getFwdRateConstants(&kf[0]);
    kin->getFwdRatesOfProgress(&ropf[0]);

    EXPECT_DOUBLE_EQ(conc[kH2O]*kf[0], ropf[0]);
    EXPECT_DOUBLE_EQ(pow(conc[kH2], 0.8)*conc[kO2]*pow(conc[kOH],2)*kf[1],
                     ropf[1]);
}

TEST_F(FracCoeffTest, CreationDestructionRates)
{
    vector_fp ropf(kin->nReactions(), 0.0);
    vector_fp cdot(therm.nSpecies(), 0.0);
    vector_fp ddot(therm.nSpecies(), 0.0);
    kin->getFwdRatesOfProgress(&ropf[0]);
    kin->getCreationRates(&cdot[0]);
    kin->getDestructionRates(&ddot[0]);

    EXPECT_DOUBLE_EQ(ropf[0], ddot[kH2O]);
    EXPECT_DOUBLE_EQ(1.4*ropf[0], cdot[kH]);
    EXPECT_DOUBLE_EQ(0.6*ropf[0], cdot[kOH]);
    EXPECT_DOUBLE_EQ(0.2*ropf[0], cdot[kO2]);

    EXPECT_DOUBLE_EQ(0.7*ropf[1]+ropf[2], ddot[kH2]);
    EXPECT_DOUBLE_EQ(0.6*ropf[1], ddot[kOH]);
    EXPECT_DOUBLE_EQ(0.2*ropf[1]+0.5*ropf[2], ddot[kO2]);
    EXPECT_DOUBLE_EQ(ropf[1]+ropf[2], cdot[kH2O]);

    EXPECT_DOUBLE_EQ(0.0, cdot[therm.speciesIndex("O")]);
    EXPECT_DOUBLE_EQ(0.0, ddot[therm.speciesIndex("O")]);
}

TEST_F(FracCoeffTest, EquilibriumConstants)
{
    vector_fp Kc(kin->nReactions(), 0.0);
    vector_fp mu0(therm.nSpecies(), 0.0);

    kin->getEquilibriumConstants(&Kc[0]);
    therm.getGibbs_ref(&mu0[0]); // at pRef

    double deltaG0_0 = 1.4 * mu0[kH] + 0.6 * mu0[kOH] + 0.2 * mu0[kO2] - mu0[kH2O];
    double deltaG0_1 = mu0[kH2O] - 0.7 * mu0[kH2] - 0.6 * mu0[kOH] - 0.2 * mu0[kO2];

    double pRef = therm.refPressure();
    double RT = therm.RT();

    // Net stoichiometric coefficients are 1.2 and -0.5
    EXPECT_NEAR(exp(-deltaG0_0/RT) * pow(pRef/RT, 1.2), Kc[0], 1e-13 * Kc[0]);
    EXPECT_NEAR(exp(-deltaG0_1/RT) * pow(pRef/RT, -0.5), Kc[1], 1e-13 * Kc[1]);
}

class NegativePreexponentialFactor : public testing::Test
{
public:
    NegativePreexponentialFactor() {}
    void setup(const std::string& infile) {
        soln = newSolution(infile);
        soln->thermo()->setState_TPX(2000, OneAtm,
            "H2O:1.0, H:0.2, O2:0.3, NH:0.05, NO:0.05, N2O:0.05");
        nSpec = soln->thermo()->nSpecies();
        nRxn = soln->kinetics()->nReactions();
    }

    void testNetProductionRates() {
        const double wdot_ref[] = {0.44705, -0.0021443, 0, -279.36, 0.0021432, 278.92, 0.4449, -279.36, 279.36, 0, 0, 0};
        ASSERT_EQ(12, (int) nSpec);
        ASSERT_EQ(12, (int) nRxn);
        vector_fp wdot(nSpec);
        soln->kinetics()->getNetProductionRates(&wdot[0]);
        for (size_t i = 0; i < nSpec; i++) {
            EXPECT_NEAR(wdot_ref[i], wdot[i], std::abs(wdot_ref[i])*2e-5 + 1e-9);
        }

        const double ropf_ref[] = {479.305, -128.202, 0, -0, 0, 0, 0, 0, 0, 0.4449, 0, 0};
        const double ropr_ref[] = {97.94, -26.1964, 0, -0, 1.10334e-06, 0, 0, 0, 6.58592e-06, 0, 0, 0.00214319};

        vector_fp ropf(nRxn);
        vector_fp ropr(nRxn);
        soln->kinetics()->getFwdRatesOfProgress(&ropf[0]);
        soln->kinetics()->getRevRatesOfProgress(&ropr[0]);
        for (size_t i = 0; i < nRxn; i++) {
            EXPECT_NEAR(ropf_ref[i], ropf[i], std::abs(ropf_ref[i])*2e-5 + 1e-9);
            EXPECT_NEAR(ropr_ref[i], ropr[i], std::abs(ropr_ref[i])*2e-5 + 1e-9);
        }
    }

    shared_ptr<Solution> soln;
    size_t nRxn, nSpec;
};

TEST_F(NegativePreexponentialFactor, fromYaml)
{
    setup("noxNeg.yaml");
    testNetProductionRates();
}

TEST(InterfaceReaction, CoverageDependency) {
    auto iface = newInterface("ptcombust.yaml", "Pt_surf");
    ASSERT_EQ(iface->kinetics()->nReactions(), (size_t) 24);

    double T = 500;
    iface->thermo()->setState_TP(T, 101325);
    iface->thermo()->setCoveragesByName("PT(S):0.7, H(S):0.3");
    vector_fp kf(iface->kinetics()->nReactions());
    iface->kinetics()->getFwdRateConstants(&kf[0]);
    EXPECT_NEAR(kf[0], 4.4579e7 * pow(T, 0.5), 1e-14*kf[0]);
    // Energies in XML file are converted from J/mol to J/kmol
    EXPECT_NEAR(kf[1], 3.7e20 * exp(-(67.4e6-6e6*0.3)/(GasConstant*T)), 1e-14*kf[1]);
}

}
