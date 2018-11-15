enum instr_size_and_shifts {
    FUNCT_SIZE    = 6,
    SHAMT_SIZE    = 5,
    RD_SIZE       = 5,
    RT_SIZE       = 5,
    RS_SIZE       = 5,
    OPCODE_SIZE   = 6,
    ASI_IMM_SIZE  = 16,
    ASJ_IMM_SIZE  = 26,

    FUNCT_SHIFT   = 0,
    SHAMT_SHIFT   = FUNCT_SHIFT + FUNCT_SIZE,
    RD_SHIFT      = SHAMT_SHIFT + SHAMT_SIZE,
    RT_SHIFT      = RD_SHIFT    + RD_SIZE,
    RS_SHIFT      = RT_SHIFT    + RT_SIZE,
    OPCODE_SHIFT  = RS_SHIFT    + RS_SIZE,
    ASI_IMM_SHIFT = 0,
    ASJ_IMM_SHIFT = 0,
};

                              //32bits          01234567012345670123456701234567
static const constexpr uint32 FUNCT_MASK    = 0b00000000000000000000000000111111;
static const constexpr uint32 SHAMT_MASK    = 0b00000000000000000000011111000000;
static const constexpr uint32 RD_MASK       = 0b00000000000000001111100000000000;
static const constexpr uint32 RT_MASK       = 0b00000000000111110000000000000000;
static const constexpr uint32 RS_MASK       = 0b00000011111000000000000000000000;
static const constexpr uint32 OPCODE_MASK   = 0b11111100000000000000000000000000;
static const constexpr uint32 ASI_IMM_MASK  = 0b00000000000000001111111111111111;
static const constexpr uint32 ASJ_IMM_MASK  = 0b00000011111111111111111111111111;

struct _instr
        {
            struct AsR
            {
                uint32 funct;
                uint32 shamt;
                uint32 rd;
                uint32 rt;
                uint32 rs;
                uint32 opcode;
            };
            struct AsI
            {
                uint32 imm;
                uint32 rt;
                uint32 rs;
                uint32 opcode;
            };
            struct AsJ
            {
                uint32 imm;
                uint32 opcode;
            };

            AsR get_as_r() const {
                AsR result = {};
                result.funct  = (raw & FUNCT_MASK)   >> FUNCT_SHIFT;
                result.shamt  = (raw & SHAMT_MASK)   >> SHAMT_SHIFT;
                result.rd     = (raw & RD_MASK)      >> RD_SHIFT;
                result.rt     = (raw & RT_MASK)      >> RT_SHIFT;
                result.rs     = (raw & RS_MASK)      >> RS_SHIFT;
                result.opcode = (raw & OPCODE_MASK)  >> OPCODE_SHIFT;
                return result;
            };
             AsI get_as_i() const {
                AsI result = {};
                result.imm    = (raw & ASI_IMM_MASK) >> ASI_IMM_SHIFT;
                result.rt     = (raw & RT_MASK)      >> RT_SHIFT;
                result.rs     = (raw & RS_MASK)      >> RS_SHIFT;
                result.opcode = (raw & OPCODE_MASK)  >> OPCODE_SHIFT;
                return result;
            };
             AsJ get_as_j() const {
                AsJ result = {};
                result.imm    = (raw & ASJ_IMM_MASK) >> ASJ_IMM_SHIFT;
                result.opcode = (raw & OPCODE_MASK)  >> OPCODE_SHIFT;
                return result;
            };

            const uint32 raw;

            _instr() : raw(NO_VAL32) { };
            explicit _instr(uint32 bytes) : raw( bytes) { }
        };

