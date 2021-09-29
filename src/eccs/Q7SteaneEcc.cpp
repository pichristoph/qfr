/*
 * This file is part of JKQ QFR library which is released under the MIT license.
 * See file README.md or go to http://iic.jku.at/eda/research/quantum/ for more information.
 */

#include "eccs/Q7SteaneEcc.hpp"

//7 data qubits, 6 for measuring -> 13 qubits per physical qubit
Q7SteaneEcc::Q7SteaneEcc(qc::QuantumComputation& qc): Ecc({EccID::Q3Shor, 13, 6, Q7SteaneEcc::getEccName()}, qc) {}

void Q7SteaneEcc::writeEccEncoding() {
	measureAndCorrect();
}

void Q7SteaneEcc::measureAndCorrect() {
    const int nQubits = qc.getNqubits();
    for(int i=0;i<nQubits;i++) {
        //
    }
}

void Q7SteaneEcc::writeEccDecoding() {
    const int nQubits = qc.getNqubits();
    for(int i=0;i<nQubits;i++) {
        //TODO
    }
}

void Q7SteaneEcc::mapGate(std::unique_ptr<qc::Operation> &gate) {
    const int nQubits = qc.getNqubits();
    int i;
    switch(gate.get()->getType()) {
    case qc::I: break;
    case qc::X:
    case qc::H:
    case qc::Y:
    case qc::Z:
    case qc::S:
    case qc::Sdag:
    case qc::T:
    case qc::Tdag:
    case qc::V:
    case qc::Vdag:
    case qc::U3:
    case qc::U2:
    case qc::Phase:
    case qc::SX:
    case qc::SXdag:
    case qc::RX:
    case qc::RY:
    case qc::RZ:
    case qc::SWAP:
    case qc::iSWAP:
    case qc::Peres:
    case qc::Peresdag:
    case qc::Compound:
    case qc::ClassicControlled:
    default:
        statistics.nOutputGates = -1;
        statistics.nOutputQubits = -1;
        throw qc::QFRException("Gate not possible to encode in error code!");
    }
}
