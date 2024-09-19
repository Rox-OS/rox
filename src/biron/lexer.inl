#ifndef KIND
#define KIND(...)
#endif

KIND(END)
KIND(AT)          // '@'
KIND(COMMA)       // ','
KIND(COLON)       // ':'
KIND(SEMI)        // ';'
KIND(LPAREN)      // '('
KIND(RPAREN)      // ')'
KIND(LBRACKET)    // '['
KIND(RBRACKET)    // ']'
KIND(LBRACE)      // '{'
KIND(RBRACE)      // '}'
KIND(PLUS)        // '+'
KIND(MINUS)       // '-'
KIND(STAR)        // '*'
KIND(FSLASH)      // '/'
KIND(PERCENT)     // '%'
KIND(NOT)         // '!'
KIND(DOLLAR)      // '$'
KIND(BOR)         // '|'
KIND(LOR)         // '||'
KIND(BAND)        // '&'
KIND(LAND)        // '&&'
KIND(DOT)         // '.'
KIND(SEQUENCE)    // '..'
KIND(ELLIPSIS)    // '...'
KIND(EQ)          // '='
KIND(EQEQ)        // '=='
KIND(NEQ)         // '!='
KIND(LT)          // '<'
KIND(MIN)         // '<?'
KIND(LTE)         // '<='
KIND(LSHIFT)      // '<<'
KIND(GT)          // '>'
KIND(MAX)         // '>?'
KIND(GTE)         // '>='
KIND(RSHIFT)      // '>>'
KIND(ARROW)       // '->'
KIND(IDENT)       // [a-z][A-Z]([a-z][A-Z][0-9]_)+
KIND(PLUSEQ)      // '+='
KIND(MINUSEQ)     // '-='
KIND(STAREQ)      // '*='
KIND(FSLASHEQ)    // '/='

KIND(KW_TRUE)     // true
KIND(KW_FALSE)    // false

KIND(KW_FN)       // 'fn'
KIND(KW_IF)       // 'if'
KIND(KW_AS)       // 'as'
KIND(KW_OF)       // 'of'
KIND(KW_LET)      // 'let'
KIND(KW_NEW)      // 'new'
KIND(KW_FOR)      // 'for'
KIND(KW_ELSE)     // 'else'
KIND(KW_TYPE)     // 'type'
KIND(KW_DEFER)    // 'defer'
KIND(KW_BREAK)    // 'break'
KIND(KW_USING)    // 'using'
KIND(KW_RETURN)   // 'return'
KIND(KW_EFFECT)   // 'effect'
KIND(KW_MODULE)   // 'module'
KIND(KW_IMPORT)   // 'import'
KIND(KW_CONTINUE) // 'continue'

// DecDigit
//  ::= '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'
// DecLiteral
//  ::= DecimalDigit DecimalDigit*
// HexDigit
//  ::= '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'
//    | 'a' | 'b' | 'c' | 'd' | 'e' | 'f' |
//    | 'A' | 'B' | 'C' | 'D' | 'E' | 'F' |
// HexLiteral
//  ::= '0x' HexDigit HexDigit*
// BinDigit
//  ::= 0 | 1
// BinLiteral
//  ::= '0b' BinaryDigit BinaryDigit*
// IntegerSuffix
//  ::= '_u8' | '_u16' | '_u32' | '_u64'
//    | '_s8' | '_s16' | '_s32' | '_s64'
// IntegerLiteral
//  ::= DecLiteral IntegerSuffix?
//    | HexLiteral IntegerSuffix?
//    | BinLiteral IntegerSuffix?
KIND(LIT_INT)

KIND(LIT_FLT)
KIND(LIT_STR)
KIND(LIT_CHR)

KIND(COMMENT)     // '//' or '/**/'

KIND(UNKNOWN)