#ifndef PARSEERROR_H //2.53 Updated to 16th Jan, 2017
#define PARSEERROR_H

#include "AST.h"
#include <string>

using std::string;

void printErrorMsg(AST *offender, int errorID, string param = string());

#define CANTOPENSOURCE 0
#define CANTOPENIMPORT 1
#define IMPORTRECURSION 2
#define IMPORTBADSCOPE 3 //DEPRECATED
#define FUNCTIONREDEF 4
#define FUNCTIONVOIDPARAM 5
#define SCRIPTREDEF 6
#define VOIDVAR 7
#define VARREDEF 8
#define VARUNDECLARED 9
#define FUNCUNDECLARED 10
#define SCRIPTNORUN 11
#define SCRIPTRUNNOTVOID 12
#define SCRIPTNUMNOTINT 13 //DEPRECATED
#define SCRIPTNUMTOOBIG 14 //DEPRECATED
#define SCRIPTNUMREDEF 15 //DEPRECATED
#define IMPLICITCAST 16
#define ILLEGALCAST 17
#define VOIDEXPR 18 //DEPRECATED
#define DIVBYZERO 19
#define CONSTTRUNC 20
#define NOFUNCMATCH 21
#define TOOFUNCMATCH 22
#define FUNCBADRETURN 23
#define TOOMANYGLOBAL 24
#define SHIFTNOTINT 25
#define REFVAR 26
#define ARROWNOTPOINTER 27
#define ARROWNOFUNC 28
#define ARROWNOVAR 29
#define TOOMANYRUN 30
#define INDEXNOTINT 31 //DEPRECATED
#define SCRIPTBADTYPE 32
#define BREAKBAD 33
#define CONTINUEBAD 34
#define CONSTREDEF 35
#define LVALCONST 36
#define BADGLOBALINIT 37
#define DEPRECATEDGLOBAL 38
#define VOIDARR 39
#define REFARR 40
#define ARRREDEF 41
#define ARRAYTOOSMALL 42
#define ARRAYLISTTOOLARGE 43
#define ARRAYLISTSTRINGTOOLARGE 44
#define NONINTEGERARRAYSIZE 45
#define EXPRNOTCONSTANT 46
#define UNRESOLVEDTYPE 47
#define CONSTUNITIALIZED 48
#define CONSTASSIGN 49
#define EMPTYARRAYLITERAL 50
#endif

