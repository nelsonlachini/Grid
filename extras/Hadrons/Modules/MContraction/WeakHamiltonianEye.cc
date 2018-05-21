/*************************************************************************************

Grid physics library, www.github.com/paboyle/Grid 

Source file: extras/Hadrons/Modules/MContraction/WeakHamiltonianEye.cc

Copyright (C) 2015-2018

Author: Antonin Portelli <antonin.portelli@me.com>
Author: Lanny91 <andrew.lawson@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

See the full license in the file "LICENSE" in the top level distribution directory
*************************************************************************************/
/*  END LEGAL */

#include <Grid/Hadrons/Modules/MContraction/WeakHamiltonianEye.hpp>

using namespace Grid;
using namespace Hadrons;
using namespace MContraction;

/*
 * Weak Hamiltonian current-current contractions, Eye-type.
 * 
 * These contractions are generated by the Q1 and Q2 operators in the physical
 * basis (see e.g. Fig 3 of arXiv:1507.03094).
 * 
 * Schematics:        q4                 |                  
 *                  /-<-¬                |                             
 *                 /     \               |             q2           q3
 *                 \     /               |        /----<------*------<----¬                        
 *            q2    \   /    q3          |       /          /-*-¬          \
 *       /-----<-----* *-----<----¬      |      /          /     \          \
 *    i *            H_W           * f   |   i *           \     /  q4      * f
 *       \                        /      |      \           \->-/          /   
 *        \                      /       |       \                        /       
 *         \---------->---------/        |        \----------->----------/        
 *                   q1                  |                   q1                  
 *                                       |
 *                Saucer (S)             |                  Eye (E)
 * 
 * S: trace(q3*g5*q1*adj(q2)*g5*gL[mu][p_1]*q4*gL[mu][p_2])
 * E: trace(q3*g5*q1*adj(q2)*g5*gL[mu][p_1])*trace(q4*gL[mu][p_2])
 * 
 * Note q1 must be sink smeared.
 */

/******************************************************************************
 *                  TWeakHamiltonianEye implementation                        *
 ******************************************************************************/
// constructor /////////////////////////////////////////////////////////////////
TWeakHamiltonianEye::TWeakHamiltonianEye(const std::string name)
: Module<WeakHamiltonianPar>(name)
{}

// dependencies/products ///////////////////////////////////////////////////////
std::vector<std::string> TWeakHamiltonianEye::getInput(void)
{
    std::vector<std::string> in = {par().q1, par().q2, par().q3, par().q4};
    
    return in;
}

std::vector<std::string> TWeakHamiltonianEye::getOutput(void)
{
    std::vector<std::string> out = {};
    
    return out;
}

// setup ///////////////////////////////////////////////////////////////////////
void TWeakHamiltonianEye::setup(void)
{
    unsigned int ndim = env().getNd();

    envTmpLat(LatticeComplex,  "expbuf");
    envTmpLat(PropagatorField, "tmp1");
    envTmpLat(LatticeComplex,  "tmp2");
    envTmp(std::vector<PropagatorField>, "S_body", 1, ndim, PropagatorField(env().getGrid()));
    envTmp(std::vector<PropagatorField>, "S_loop", 1, ndim, PropagatorField(env().getGrid()));
    envTmp(std::vector<LatticeComplex>,  "E_body", 1, ndim, LatticeComplex(env().getGrid()));
    envTmp(std::vector<LatticeComplex>,  "E_loop", 1, ndim, LatticeComplex(env().getGrid()));
}

// execution ///////////////////////////////////////////////////////////////////
void TWeakHamiltonianEye::execute(void)
{
    LOG(Message) << "Computing Weak Hamiltonian (Eye type) contractions '" 
                 << getName() << "' using quarks '" << par().q1 << "', '" 
                 << par().q2 << ", '" << par().q3 << "' and '" << par().q4 
                 << "'." << std::endl;

    ResultWriter           writer(RESULT_FILE_NAME(par().output));
    auto                   &q1 = envGet(SlicedPropagator, par().q1);
    auto                   &q2 = envGet(PropagatorField, par().q2);
    auto                   &q3 = envGet(PropagatorField, par().q3);
    auto                   &q4 = envGet(PropagatorField, par().q4);
    Gamma                  g5  = Gamma(Gamma::Algebra::Gamma5);
    std::vector<TComplex>  corrbuf;
    std::vector<Result>    result(n_eye_diag);
    unsigned int ndim    = env().getNd();

    envGetTmp(LatticeComplex,               expbuf); 
    envGetTmp(PropagatorField,              tmp1);
    envGetTmp(LatticeComplex,               tmp2);
    envGetTmp(std::vector<PropagatorField>, S_body);
    envGetTmp(std::vector<PropagatorField>, S_loop);
    envGetTmp(std::vector<LatticeComplex>,  E_body);
    envGetTmp(std::vector<LatticeComplex>,  E_loop);

    // Get sink timeslice of q1.
    SitePropagator q1Snk = q1[par().tSnk];

    // Setup for S-type contractions.
    for (int mu = 0; mu < ndim; ++mu)
    {
        S_body[mu] = MAKE_SE_BODY(q1Snk, q2, q3, GammaL(Gamma::gmu[mu]));
        S_loop[mu] = MAKE_SE_LOOP(q4, GammaL(Gamma::gmu[mu]));
    }

    // Perform S-type contractions.    
    SUM_MU(expbuf, trace(S_body[mu]*S_loop[mu]))
    MAKE_DIAG(expbuf, corrbuf, result[S_diag], "HW_S")

    // Recycle sub-expressions for E-type contractions.
    for (unsigned int mu = 0; mu < ndim; ++mu)
    {
        E_body[mu] = trace(S_body[mu]);
        E_loop[mu] = trace(S_loop[mu]);
    }

    // Perform E-type contractions.
    SUM_MU(expbuf, E_body[mu]*E_loop[mu])
    MAKE_DIAG(expbuf, corrbuf, result[E_diag], "HW_E")

    write(writer, "HW_Eye", result);
}
