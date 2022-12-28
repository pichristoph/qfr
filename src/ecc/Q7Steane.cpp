/*
* This file is part of MQT QFR library which is released under the MIT license.
* See file README.md or go to https://www.cda.cit.tum.de/research/quantum/ for more information.
*/

#include "ecc/Q7Steane.hpp"
namespace ecc {
    void Q7Steane::writeEncoding() {
        if (!isDecoded) {
            return;
        }
        isDecoded          = false;
        const auto nQubits = qcOriginal->getNqubits();
        //reset data qubits if necessary
        if (gatesWritten) {
            for (std::size_t i = 0; i < nQubits; i++) {
                for (std::size_t j = 1; j < 7; j++) {
                    qcMapped->reset(static_cast<Qubit>(i + j * nQubits));
                }
            }
        }
        measureAndCorrectSingle(true);
    }

    void Q7Steane::measureAndCorrect() {
        if (isDecoded) {
            return;
        }
        measureAndCorrectSingle(true);
        measureAndCorrectSingle(false);
    }

    void Q7Steane::measureAndCorrectSingle(bool xSyndrome) {
        const auto nQubits         = qcOriginal->getNqubits();
        const auto ancStart        = nQubits * ecc.nRedundantQubits;
        const auto clAncStart      = static_cast<QubitCount>(qcOriginal->getNcbits());
        const auto controlRegister = std::make_pair(static_cast<Qubit>(clAncStart), static_cast<QubitCount>(3));

        for (Qubit i = 0; i < nQubits; i++) {
            if (gatesWritten) {
                for (std::size_t j = 0; j < ecc.nCorrectingBits; j++) {
                    qcMapped->reset(static_cast<Qubit>(ancStart + j));
                }
            }

            std::array<qc::Control, 3> controls = {};
            for (std::size_t j = 0; j < ecc.nCorrectingBits; j++) {
                qcMapped->h(static_cast<Qubit>(ancStart + j));
                controls[j] = qc::Control{static_cast<Qubit>(ancStart + j), qc::Control::Type::Pos};
            }

            staticWriteFunctionType writeXZ = xSyndrome ? Ecc::x : Ecc::z;

            //K1: UIUIUIU
            //K2: IUUIIUU
            //K3: IIIUUUU
            for (std::size_t c = 0; c < controls.size(); c++) {
                for (std::size_t q = 0; q < ecc.nRedundantQubits; q++) {
                    if (((q + 1) & (1 << c)) != 0) {
                        writeXZ(static_cast<Qubit>(i + nQubits * q), controls[c], qcMapped);
                    }
                }
            }

            for (std::size_t j = 0; j < ecc.nCorrectingBits; j++) {
                qcMapped->h(static_cast<Qubit>(ancStart + j));
                qcMapped->measure(static_cast<Qubit>(ancStart + j), clAncStart + j);
            }

            //correct Z_i for i+1 = c0*1+c1*2+c2*4
            //correct X_i for i+1 = c3*1+c4*2+c5*4
            for (std::size_t j = 0; j < 7; j++) {
                classicalControl(controlRegister, j + 1U, xSyndrome ? qc::Z : qc::X, static_cast<Qubit>(i + j * nQubits));
            }
            gatesWritten = true;
        }
    }

    void Q7Steane::writeDecoding() {
        if (isDecoded) {
            return;
        }
        const auto                            nQubits          = qcOriginal->getNqubits();
        const auto                            clAncStart       = static_cast<QubitCount>(qcOriginal->getNcbits());
        static constexpr std::array<Qubit, 4> correctionNeeded = {1, 2, 4, 7}; //values with odd amount of '1' bits
        //use exiting registers qeccX and qeccZ for decoding
        const auto controlRegister = std::make_pair(static_cast<Qubit>(clAncStart), static_cast<QubitCount>(3));

        for (Qubit i = 0; i < nQubits; i++) {
            //#|###|###
            //0|111|111
            //odd amount of 1's -> x[0] = 1
            //measure from index 1 (not 0) to 6, =qubit 2 to 7

            qcMapped->measure(static_cast<Qubit>(i + 1 * nQubits), clAncStart);
            qcMapped->measure(static_cast<Qubit>(i + 2 * nQubits), clAncStart + 1);
            qcMapped->measure(static_cast<Qubit>(i + 3 * nQubits), clAncStart + 2);
            for (auto value: correctionNeeded) {
                classicalControl(controlRegister, value, qc::X, i);
            }
            qcMapped->measure(static_cast<Qubit>(i + 4 * nQubits), clAncStart);
            qcMapped->measure(static_cast<Qubit>(i + 5 * nQubits), clAncStart + 1);
            qcMapped->measure(static_cast<Qubit>(i + 6 * nQubits), clAncStart + 2);
            for (auto value: correctionNeeded) {
                classicalControl(controlRegister, value, qc::X, i);
            }
        }
        isDecoded = true;
    }

    void Q7Steane::mapGate(const qc::Operation& gate) {
        if (isDecoded && gate.getType() != qc::Measure) {
            writeEncoding();
        }
        const QubitCount nQubits = qcOriginal->getNqubits();
        switch (gate.getType()) {
            case qc::I:
                break;
            case qc::X:
            case qc::H:
            case qc::Y:
            case qc::Z:
                for (auto i: gate.getTargets()) {
                    if (gate.getNcontrols() != 0U) {
                        const auto& ctrls = gate.getControls();
                        for (std::size_t j = 0; j < 7; j++) {
                            qc::Controls ctrls2;
                            for (const auto& ct: ctrls) {
                                ctrls2.insert(qc::Control{static_cast<Qubit>(ct.qubit + j * nQubits), ct.type});
                            }
                            qcMapped->emplace_back<qc::StandardOperation>(qcMapped->getNqubits(), ctrls2, static_cast<Qubit>(i + j * nQubits), gate.getType());
                        }
                    } else {
                        for (std::size_t j = 0; j < 7; j++) {
                            qcMapped->emplace_back<qc::StandardOperation>(qcMapped->getNqubits(), static_cast<Qubit>(i + j * nQubits), gate.getType());
                        }
                    }
                }
                break;
                //locigal S = 3 physical S's
            case qc::S:
            case qc::Sdag:
                for (auto i: gate.getTargets()) {
                    if (gate.getNcontrols() != 0U) {
                        const auto& ctrls = gate.getControls();
                        for (std::size_t j = 0; j < 7; j++) {
                            qc::Controls ctrls2;
                            for (const auto& ct: ctrls) {
                                ctrls2.insert(qc::Control{static_cast<Qubit>(ct.qubit + j * nQubits), ct.type});
                            }
                            qcMapped->emplace_back<qc::StandardOperation>(qcMapped->getNqubits(), ctrls2, static_cast<Qubit>(i + j * nQubits), gate.getType());
                            qcMapped->emplace_back<qc::StandardOperation>(qcMapped->getNqubits(), ctrls2, static_cast<Qubit>(i + j * nQubits), gate.getType());
                            qcMapped->emplace_back<qc::StandardOperation>(qcMapped->getNqubits(), ctrls2, static_cast<Qubit>(i + j * nQubits), gate.getType());
                        }
                    } else {
                        for (std::size_t j = 0; j < 7; j++) {
                            qcMapped->emplace_back<qc::StandardOperation>(qcMapped->getNqubits(), static_cast<Qubit>(i + j * nQubits), gate.getType());
                            qcMapped->emplace_back<qc::StandardOperation>(qcMapped->getNqubits(), static_cast<Qubit>(i + j * nQubits), gate.getType());
                            qcMapped->emplace_back<qc::StandardOperation>(qcMapped->getNqubits(), static_cast<Qubit>(i + j * nQubits), gate.getType());
                        }
                    }
                }
                break;
            case qc::Measure:
                if (!isDecoded) {
                    measureAndCorrect();
                    writeDecoding();
                }
                if (const auto* measureGate = dynamic_cast<const qc::NonUnitaryOperation*>(&gate)) {
                    for (std::size_t j = 0; j < measureGate->getNclassics(); j++) {
                        auto classicalRegisterName = qcOriginal->getClassicalRegister(measureGate->getTargets()[j]);
                        if (!classicalRegisterName.empty()) {
                            qcMapped->measure(static_cast<Qubit>(measureGate->getClassics()[j]), {classicalRegisterName, measureGate->getTargets()[j]});
                        } else {
                            qcMapped->measure(static_cast<Qubit>(measureGate->getClassics()[j]), measureGate->getTargets()[j]);
                        }
                    }
                } else {
                    throw std::runtime_error("Dynamic cast to NonUnitaryOperation failed.");
                }

                break;
            case qc::T:
            case qc::Tdag:
                for (auto i: gate.getTargets()) {
                    if (gate.getControls().empty()) {
                        qcMapped->x(static_cast<Qubit>(i + 5 * nQubits), qc::Control{static_cast<Qubit>(i + 6 * nQubits), qc::Control::Type::Pos});
                        qcMapped->x(static_cast<Qubit>(i + 0 * nQubits), qc::Control{static_cast<Qubit>(i + 5 * nQubits), qc::Control::Type::Pos});
                        if (gate.getType() == qc::T) {
                            qcMapped->t(static_cast<Qubit>(i + 0 * nQubits));
                        } else {
                            qcMapped->tdag(static_cast<Qubit>(i + 0 * nQubits));
                        }
                        qcMapped->x(static_cast<Qubit>(i + 0 * nQubits), qc::Control{static_cast<Qubit>(i + 5 * nQubits), qc::Control::Type::Pos});
                        qcMapped->x(static_cast<Qubit>(i + 5 * nQubits), qc::Control{static_cast<Qubit>(i + 6 * nQubits), qc::Control::Type::Pos});
                    } else {
                        gateNotAvailableError(gate);
                    }
                }
                break;
            default:
                gateNotAvailableError(gate);
        }
    }
} // namespace ecc
