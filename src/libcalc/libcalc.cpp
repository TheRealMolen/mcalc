#include "libcalc.h"

#include "expr.h"
#include "funcs.h"
#include "parser.h"

#include <cstdio>
#include <cstring>

//-------------------------------------------------------------------------------------------------

void dtostr_human(double d, char* s, int sLen)
{
    snprintf(s, sLen, "    = %.8g", d);
}

//-------------------------------------------------------------------------------------------------

// f: x-> expression
// statement ::= symbol ":" symbol "->" expression
// todo: arg list as symbol_list
bool parse_statement(ParseCtx& ctx)
{
    char name[kMaxSymbolLength+1];
    char arg[kMaxSymbolLength+1];

    if (!expect_symbol(ctx, name))
        return false;
    if (!expect(ctx, Token::Assign))
        return false;
    if (!expect_symbol(ctx, arg))
        return false;
    if (!peek(ctx, Token::Map))
        return false;

    // the remainder of the expression becomes the registered implementation of function <name>
    ParseCtx innerCtx {
        .InBuffer = ctx.InBuffer + ctx.CurrIx,
        .ResBuffer = ctx.ResBuffer,
        .ResBufferLen = ctx.ResBufferLen
    };
    if (!define_function(name, arg, innerCtx))
        return false;

    // we've eaten all the rest of the input
    ctx.NextToken = Token::Eof;

    return true;
}

//-------------------------------------------------------------------------------------------------

bool calc_eval(const char* expr, char* resBuffer, int resBufferLen)
{
    if (!resBuffer)
        return false;
    *resBuffer = 0;

    ParseCtx parseCtx { .InBuffer=expr, .ResBuffer=resBuffer, .ResBufferLen=resBufferLen };
    advance_token(parseCtx);

    // scan the expression to see if there's a definition symbol in there
    const bool isStatement = (strstr(expr, "->") != nullptr);

    double result = 0.0;

    if (isStatement)
    {
        if (parse_statement(parseCtx))
        {
            strcpy(resBuffer, "   ok.");
        }
    }
    else
    {
        result = parse_expression(parseCtx);
    }

    if (!accept(parseCtx, Token::Eof))
        on_parse_error(parseCtx, "trailing nonsense");
    
    if (parseCtx.Error)
        return false;

    if (!isStatement)
        dtostr_human(result, resBuffer, resBufferLen);

    return true;
}



