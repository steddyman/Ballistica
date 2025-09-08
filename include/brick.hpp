// brick.hpp - Enumerated brick types matching legacy shorthand order
#pragma once

enum class BrickType : int {
    NB=0, YB, GB, CB, TB, PB, RB, LB, SB, FB, F1, F2, B1, B2, B3, B4, B5,
    BS, BB, ID, RW, RE, IS, IF, AB, FO, LA, MB, BA, T5, BO, OF, ON, SS, SF,
    COUNT
};

inline bool brick_is_breakable(BrickType t) {
    switch(t) {
        case BrickType::NB: case BrickType::ID: case BrickType::OF: case BrickType::ON: return false;
        default: return true;
    }
}
