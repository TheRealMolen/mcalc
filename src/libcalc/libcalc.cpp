#include "libcalc.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>


enum class Token
{
    Invalid, Eof,
    Number,

    // note that these are in order of precedence
    Plus, Minus,
    Times, Divide,
    Exponent,
    LParen, RParen,
};


struct ParseCtx
{
    const char* InBuffer = nullptr;
    int CurrIx = 0;

    char* ResBuffer = nullptr;
    int ResBufferLen = 0;
    bool Error = false;

    Token NextToken = Token::Invalid;
    double TokenNumber = 0.f;
};


void on_parse_error(ParseCtx& ctx, const char* msg)
{
    // only one error at a time pls
    if (ctx.Error)
        return;

    ctx.Error = true;

    if (!ctx.ResBuffer)
        return;

    char* resBufEnd = ctx.ResBuffer + (ctx.ResBufferLen - 1);
    char* resCurr = ctx.ResBuffer;
    if (msg)
    {
        for (const char* in=msg; *in && resCurr < resBufEnd; ++in, ++resCurr)
            *resCurr = *in;

        if (resCurr < resBufEnd)
            *(resCurr++) = '\n';
    }

    for (const char* in=ctx.InBuffer; *in && resCurr < resBufEnd; ++in, ++resCurr)
        *resCurr = *in;
    if (resCurr < resBufEnd)
        *(resCurr++) = '\n';

    for (int i=0; i<ctx.CurrIx && resCurr < resBufEnd; ++i, ++resCurr)
        *resCurr = ' ';
    if (resCurr < resBufEnd)
        *(resCurr++) = '^';
    if (resCurr < resBufEnd)
        *(resCurr++) = '\n';
    if (resCurr < resBufEnd)
        *(resCurr++) = '0';
    
    resBufEnd = 0;
}

void parse_number(ParseCtx& ctx)
{
    const char* start = ctx.InBuffer + ctx.CurrIx;
    char* end = nullptr;

    errno = 0;
    ctx.TokenNumber = strtod(start, &end);
    ctx.NextToken = Token::Number;

    if (end)
        ctx.CurrIx = end - ctx.InBuffer;

    if (errno)
    {
        on_parse_error(ctx, "scary number");
        ctx.NextToken = Token::Invalid;
    }
}

void advance_token(ParseCtx& ctx)
{
    const char c = ctx.InBuffer[ctx.CurrIx];

    switch (c)
    {
    case 0:
        ctx.NextToken = Token::Eof;
        return; // nb. return early so we don't increment currIx

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case '.':
        parse_number(ctx);
        return; // nb. return early so we don't increment currIx again

    case '*':
        if (ctx.InBuffer[ctx.CurrIx + 1] == '*')
        {
            ctx.NextToken = Token::Exponent;
            ++ctx.CurrIx;
        }
        else
        {
            ctx.NextToken = Token::Times;
        }
        break;

    case '+': ctx.NextToken = Token::Plus;      break;
    case '-': ctx.NextToken = Token::Minus;     break;
    case '/': ctx.NextToken = Token::Divide;    break;
    case '(': ctx.NextToken = Token::LParen;    break;
    case ')': ctx.NextToken = Token::RParen;    break;

    default:
        ctx.NextToken = Token::Invalid;
    }

    ++ctx.CurrIx;
}

//-----------------------------------------------------------------------------------------------

bool accept(ParseCtx& ctx, Token t)
{
    if (ctx.NextToken == t)
    {
        advance_token(ctx);
        return true;
    }
    return false;
}

bool expect(ParseCtx& ctx, Token t)
{
    if (ctx.Error)
        return false;

    if (accept(ctx, t))
        return true;

    on_parse_error(ctx, "unexpected token");
    return false;
}

double expect_number(ParseCtx& ctx)
{
    if (ctx.Error)
        return 0.0;

    if (ctx.NextToken == Token::Number)
    {
        double val = ctx.TokenNumber;
        advance_token(ctx);
        return val;
    }

    on_parse_error(ctx, "expected number");
    return 0;
}

bool peek(const ParseCtx& ctx, Token t)
{
    if (ctx.Error)
        return false;

    return ctx.NextToken == t;
}

//-------------------------------------------------------------------------------------------------

double parse_expression(ParseCtx& ctx);

// primary = number | "(" expression ")"
double parse_primary(ParseCtx& ctx)
{
    if (accept(ctx, Token::LParen))
    {
        double val = parse_expression(ctx);
        expect(ctx, Token::RParen);
        return val;
    }

    return expect_number(ctx);
}

// exponent ::= primary [ "**" primary ]
double parse_exponent(ParseCtx& ctx)
{
    double val = parse_primary(ctx);
    if (ctx.Error)
        return 0;

    if (accept(ctx, Token::Exponent))
    {
        double power = parse_primary(ctx);
        if (ctx.Error)
            return 0;

        val = std::pow(val, power);
    }

    return val;
}

// unary = exponent | "+" unary | "-" unary
double parse_unary(ParseCtx& ctx)
{
    if (accept(ctx, Token::Plus))
        return parse_unary(ctx);
    if (accept(ctx, Token::Minus))
        return -1.0 * parse_unary(ctx);

    return parse_exponent(ctx);
}

// mul ::= unary | mul "*" unary | mul "/" unary
double parse_mul(ParseCtx& ctx)
{
    double val = parse_unary(ctx);

    while (!ctx.Error && (peek(ctx, Token::Times) || peek(ctx, Token::Divide)))
    {
        if (accept(ctx, Token::Times))
        {
            val = val * parse_unary(ctx);
        }
        else if (accept(ctx, Token::Divide))
        {
            val = val / parse_unary(ctx);
        }
    }

    return val;
}

// add ::= mul | add "+" mul | add "-" mul
double parse_add(ParseCtx& ctx)
{
    double val = parse_mul(ctx);

    while (!ctx.Error && (peek(ctx, Token::Plus) || peek(ctx, Token::Minus)))
    {
        if (accept(ctx, Token::Plus))
        {
            val = val + parse_mul(ctx);
        }
        else if (accept(ctx, Token::Minus))
        {
            val = val - parse_mul(ctx);
        }
    }

    return val;
}


double parse_expression(ParseCtx& ctx)
{
    return parse_add(ctx);
}


bool calc_eval(const char* expr, char* resBuffer, int resBufferLen)
{
    ParseCtx parseCtx { .InBuffer=expr, .ResBuffer=resBuffer, .ResBufferLen=resBufferLen };
    advance_token(parseCtx);

    double result = parse_expression(parseCtx);
    if (parseCtx.Error)
        return false;

    snprintf(resBuffer, resBufferLen, "%f", result);

    return true;
}



