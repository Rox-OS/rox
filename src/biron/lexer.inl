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
KIND(LTE)         // '<='
KIND(LSHIFT)      // '<<'
KIND(GT)          // '>'
KIND(GTE)         // '>='
KIND(RSHIFT)      // '>>'
KIND(ARROW)       // '->'
KIND(IDENT)       // [a-z][A-Z]([a-z][A-Z][0-9]_)+

KIND(KW_TRUE)     // true
KIND(KW_FALSE)    // false

KIND(KW_FN)       // 'fn'
KIND(KW_IF)       // 'if'
KIND(KW_AS)       // 'as'
KIND(KW_LET)      // 'let'
KIND(KW_FOR)      // 'for'
KIND(KW_ELSE)     // 'else'
KIND(KW_TYPE)     // 'type'
KIND(KW_DEFER)    // 'defer'
KIND(KW_UNION)    // 'union'
KIND(KW_BREAK)    // 'break'
KIND(KW_RETURN)   // 'return'
KIND(KW_CONTINUE) // 'continue'

KIND(LIT_INT)
KIND(LIT_FLT)
KIND(LIT_STR)
KIND(LIT_CHR)

KIND(COMMENT)     // '//' or '/**/'

KIND(UNKNOWN)