#include "funcs.h"

#include "expr.h"
#include "parser.h"
#include "symbols.h"

#include <cmath>
#include <cstring>

//-----------------------------------------------------------------------------------------------

constexpr int kMaxFuncDefLen = 255;
constexpr int kMaxUserFuncs = 10;

//-----------------------------------------------------------------------------------------------

typedef double (*CalcDoubleFn)(double);

// a function is defined strictly as taking zero or more args and returning a single value
// TODO: allow complex/fractional/vector/matrix return vals
struct FunctionDef
{
    const char* Name = nullptr;
    const char* Args = "d";
    CalcDoubleFn FuncPtr = nullptr;
};

struct UserFunction
{
    char Name[kMaxSymbolLength+1] = {0};
    char Arg[kMaxSymbolLength+1] = {0};

    char Def[kMaxFuncDefLen+1] = {0};

    bool IsUsed = false;
};

//-----------------------------------------------------------------------------------------------

FunctionDef gFunctions[] =
{
    { .Name = "sin", .FuncPtr = (CalcDoubleFn)sin },
    { .Name = "cos", .FuncPtr = (CalcDoubleFn)cos },
    { .Name = "tan", .FuncPtr = (CalcDoubleFn)tan },

    { .Name = "asin", .FuncPtr = (CalcDoubleFn)asin },
    { .Name = "acos", .FuncPtr = (CalcDoubleFn)acos },
    { .Name = "atan", .FuncPtr = (CalcDoubleFn)atan },

    { .Name = "ln", .FuncPtr = (CalcDoubleFn)log },
    { .Name = "log", .FuncPtr = (CalcDoubleFn)log10 },
    { .Name = "sqrt", .FuncPtr = (CalcDoubleFn)sqrt },
};
constexpr int kNumFunctions = sizeof(gFunctions) / sizeof(gFunctions[0]);


UserFunction gUserFuncs[kMaxUserFuncs];

//-----------------------------------------------------------------------------------------------

static UserFunction* find_or_alloc_userfunc(const char* name)
{
    UserFunction* free_func = nullptr;

    for (UserFunction& func : gUserFuncs)
    {
        if (!func.IsUsed)
        {
            if (!free_func)
                free_func = &func;
            continue;
        }

        if (strcmp(name, func.Name) == 0)
            return &func;
    }

    if (free_func)
    {
        strcpy(free_func->Name, name);
        free_func->IsUsed = true;
        return free_func;
    }

    return nullptr;
}

bool define_function(const char* name, const char* arg, ParseCtx& ctx)
{
    UserFunction* func = find_or_alloc_userfunc(name);
    if (!func)
    {
        on_parse_error(ctx, "too many user funcs");
        return false;
    }

    strcpy(func->Arg, arg);

    if (strlen(ctx.InBuffer) > kMaxFuncDefLen)
    {
        on_parse_error(ctx, "function def too long");
        return false;
    }

    strcpy(func->Def, ctx.InBuffer);
    return true;
}

//-----------------------------------------------------------------------------------------------

bool eval_function(const char* name, double arg1, double& outVal, ParseCtx& ctx)
{
    for (const FunctionDef& func : gFunctions)
    {
        if (strcmp(func.Name, name) == 0)
        {
            outVal = func.FuncPtr(arg1);
            return true;
        }
    }

    for (const UserFunction& func : gUserFuncs)
    {
        if (func.IsUsed && (strcmp(func.Name, name) == 0))
        {
            define_value(func.Arg, arg1, ctx);

            ParseCtx innerCtx { .InBuffer = func.Def, .ResBuffer = ctx.ResBuffer, .ResBufferLen = ctx.ResBufferLen };
            advance_token(innerCtx);
            outVal = parse_expression(innerCtx);

            undef_value(func.Arg);

            return !innerCtx.Error;
        }
    }

    outVal = 0.0;
    return false;
}

//-----------------------------------------------------------------------------------------------

bool is_user_func(const char* name)
{
    for (const UserFunction& func : gUserFuncs)
    {
        if (func.IsUsed && (strcmp(func.Name, name) == 0))
            return true;
    }

    return false;
}

//-----------------------------------------------------------------------------------------------

