#pragma once

struct ParseCtx;


bool eval_function(const char* name, double arg1, double& outVal, ParseCtx& ctx);

bool define_function(const char* name, const char* arg, ParseCtx& ctx);

bool is_user_func(const char* name);


