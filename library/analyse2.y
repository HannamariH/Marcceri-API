%{
#pragma warning( disable : 4102 )

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <time.h>

/* Command to build:  ../bison+.pl -l analyse2.y MarcScanner > uni */

/*headerdef
#include "typedef.h"
*/

/*classdef
private:
  virtual const char* NextSubField(TypeCD*, TypeCD*) = 0;
  virtual const char* LastSubField(TypeCD*,TypeCD*) = 0;
  virtual int Precedes(TypeCD*, TypeCD*) = 0;
  virtual int Exists(TypeCD*) = 0;
  virtual const char* LireCD(TypeCD*) = 0;
  virtual const char* NextBalise() = 0;
  virtual const char* PreviousBalise() = 0;
  virtual TypeCD* AllocCD() = 0;
  virtual void FreeCD( TypeCD* CD ) = 0;
  virtual TypeInst* Next_( TypeCD* cd1, TypeCD* cd2, int strict ) = 0;
  virtual TypeInst* Last_( TypeCD* cd1, TypeCD* cd2, int strict ) = 0;
  virtual TypeInst* NextBal() = 0;
  virtual TypeInst* PreviousBal() = 0;
  virtual TypeInst* Soust( TypeInst* t1, TypeInst* t2 ) = 0;
  virtual TypeInst* Multi( TypeInst* t1, TypeInst* t2 ) = 0;
  virtual TypeInst* Divis( TypeInst* t1, TypeInst* t2 ) = 0;
  virtual TypeInst* Value( TypeInst* t ) = 0;
  virtual int MemSto( TypeInst* n ) = 0;
  virtual TypeInst* MemMem( TypeInst* n ) = 0;
  virtual int MemClr( TypeInst* n  ) = 0;
  virtual TypeInst* MemExc( TypeInst* n ) = 0;
*/


%}

%union {
        int   code;
        TypeInst* inst;
        TypeCD*   tcd;
}


%token <code> SEP FIN WNUMBER WSTRING
%token <code> PLUS MOINS MULTIPLIE DIVISE
%token <code> EQ NE _IN GT LT GE LE
%token <code> EXISTS PRECEDES FOLLOWS
%token <code> IF THEN ELSE AND OR NOT
%token <code> BY _STRICT AT BEGINING END BOTH

%token <inst> VARS VARD STRING NUMERIC
%token <inst> VAR_N VAR_NT VAR_NS VAR_NO VAR_NTO VAR_NSO
%token <inst> TAG STAG FIX I1 I2
%token <inst> STR VAL LEN STO MEM EXC CLR LOWER UPPER
%token <inst> FROM TO BETWEEN _DELETE REPLACE REPLACEOCC
%token <inst> BFIRST EFIRST BLAST ELAST
%token <inst> REDO SORT NEXT LAST TABLE ORDINAL
%token <inst> YEAR MONTH DAY HOUR MINUTE SECOND
%token <inst> NEXTBAL PREVIOUSBAL REGFIND REGMATCH REGREPLACE

%left  SEP PLUS MOINS MULTIPLIE DIVISE

%type <inst> Program Rules Rule
%type <inst> SetOfInstr
%type <inst> Instruction Condition Translation
%type <tcd>  CD TAGOCC STAGOCC
%type <code> Boolean

%%
Program :
        Rules                   {
                                  if (!RedoFlag)
                                  {
                                        FreeTypeInst(S);
                                        Copie(&S,$$);
                                        FreeTypeInst($$);
                                        $$=NULL;
                                        return 1;
                                  }
                                  else
                                  {
                                        FreeTypeInst(S);
                                        S=T;
                                        Copie(&T,$$);
                                        FreeTypeInst($$);
                                        $$=NULL;
                                        if (T->str.str()[0]==0) return 1;
                                        else              return 3;
                                  }
                                }
;

Rules :
        Rule                    {
                                  Copie(&$$,S);
                                  FreeTypeInst(S);
                                  S=NULL;
                                }
|       PLUS Rule               {
                                  $$=Ajout(D,S);
                                  D=S=NULL;
                                }
;

Rule :
        SetOfInstr FIN
|       SetOfInstr SEP FIN
;

SetOfInstr :
        SetOfInstr SEP SetOfInstr
|       Instruction             {
                                  FreeTypeInst(S);
                                  Copie(&S,$1);
                                  FreeTypeInst($1);
                                  $1=NULL;
                                }
;

Instruction :
        Condition
|       Translation
|       REDO                    { PrintDebug("Redo");
                                  Copie(&$$,S);
                                  RedoFlag=1;
                                  Copie(&T,S);
/*                                ++NO->val;
                                  if (NTO->val>=0) ++NTO->val;
                                  else
                                  if (NSO->val>=0) ++NSO->val;*/
                                }
;

CD :
        TAGOCC                  { PrintDebug("Tagocc");
                                  $$=$1;
                                  $$->Fixed.freestr();
                                  $$->SubField[0]=0;
                                  $$->ns=0;
                                }
|       TAGOCC STAGOCC          { PrintDebug("Tagocc Stagocc");
                                  $$=$1;
                                  $$->Fixed.freestr();
                                  strcpy($$->SubField,$2->SubField);
                                  $$->ns=$2->ns;
                                  FreeCD($2);
                                  $2=NULL;}
|       TAGOCC FIX              { PrintDebug("Tagocc Fix");
                                  $$=$1;
                                  $$->SubField[0]=0;
                                  $$->ns=0;
                                  $$->Fixed.str($2->str.str());
                                  FreeTypeInst($2);
                                  $2=NULL;}
|       TAGOCC STAGOCC FIX      { PrintDebug("Tagocc Stagocc Fix");
                                  $$=$1;
                                  strcpy($$->SubField,$2->SubField);
                                  $$->ns=$2->ns;
                                  $$->Fixed.str($3->str.str());
                                  FreeCD($2);
                                  $2=NULL;
                                  FreeTypeInst($3);
                                  $3=NULL;}
|       STAGOCC                 { PrintDebug("Stagocc");
                                  $$=$1;
                                  $$->Fixed.freestr();
                                  strcpy($$->Field,CDIn->Field);
                                  $$->nt=0; }
|       FIX                     { PrintDebug("Fix");
                                  $$=AllocCD();
                                  strcpy($$->Field,CDIn->Field);
                                  $$->nt=0;
                                  strcpy($$->SubField,CDIn->SubField);
                                  $$->ns=0;
                                  $$->Fixed.str($1->str.str());
                                  FreeTypeInst($1);
                                  $1=NULL;}
|       STAGOCC FIX             { PrintDebug("Stagocc Fix");
                                  $$=$1;
                                  $$->Fixed.str($2->str.str());
                                  strcpy($$->Field,CDIn->Field);
                                  $$->nt=0;
                                  FreeTypeInst($2);
                                  $2=NULL;}
;

TAGOCC :
        TAG                             { PrintDebug("Tag");
                                          $$=AllocCD();
                                          strcpy($$->Field,$1->str.str());
                                          $$->nt=0;
                                          FreeTypeInst($1);
                                          $1=NULL; }
|       TAG '(' Translation ')'         { PrintDebug("Tag(...)");
                                          $$=AllocCD();
                                          strcpy($$->Field,$1->str.str());
                                          FreeTypeInst($1);
                                          $1=NULL;
                                          $$->nt=$3->val;
                                          FreeTypeInst($3);
                                          $3=NULL; }
;

STAGOCC :
        STAG                            { PrintDebug("Stag");
                                          $$=AllocCD();
                                          strcpy($$->SubField,$1->str.str());
                                          $$->ns=0;
                                          FreeTypeInst($1);
                                          $1=NULL; }
|       STAG '(' Translation ')'        { PrintDebug("Stag(...)");
                                          $$=AllocCD();
                                          strcpy($$->SubField,$1->str.str());
                                          FreeTypeInst($1);
                                          $1=NULL;
                                          $$->ns=$3->val;
                                          FreeTypeInst($3);
                                          $3=NULL; }
|       I1                              { PrintDebug("I1");
                                          $$=AllocCD();
                                          strcpy($$->SubField,"I1");
                                          $$->ns=0;
                                        }
|       I2                              { PrintDebug("I2");
                                          $$=AllocCD();
                                          strcpy($$->SubField,"I2");
                                          $$->ns=0;
                                        }
;

Condition :
        IF Boolean THEN Translation     { PrintDebug("If...Then...");
                                          if ($2) $$=$4;
                                          else
                                           {
                                            FreeTypeInst($4);
                                            $4=NULL;
                                            FreeTypeInst(S);
                                            S=NULL;
                                            return 2;
                                           }
                                        }
|       IF Boolean THEN Translation ELSE Translation
                                        { PrintDebug("If...Then...Else...");
                                          if ($2)
                                          {
                                                $$=$4;
                                                FreeTypeInst($6);
                                                $6=NULL;
                                          }
                                          else
                                          {
                                                $$=$6;
                                                FreeTypeInst($4);
                                                $4=NULL;
                                          }
                                        }
|       IF Boolean THEN Condition     { PrintDebug("If...Then... (condition)");
                                          if ($2) $$=$4;
                                          else
                                           {
                                            FreeTypeInst($4);
                                            $4=NULL;
                                            FreeTypeInst(S);
                                            S=NULL;
                                            return 2;
                                           }
                                        }
|       IF Boolean THEN Translation ELSE Condition
                                        { PrintDebug("If...Then...Else... (condition)");
                                          if ($2)
                                          {
                                                $$=$4;
                                                FreeTypeInst($6);
                                                $6=NULL;
                                          }
                                          else
                                          {
                                                $$=$6;
                                                FreeTypeInst($4);
                                                $4=NULL;
                                          }
                                        }
;

Boolean :
        '(' Boolean ')'                 { PrintDebug("(B...)"); $$=$2; }
|       Boolean AND Boolean             { PrintDebug("B...and B..."); $$=$1 && $3; }
|       Boolean OR Boolean              { PrintDebug("B...or B...");$$=$1 || $3; }
|       NOT Boolean                     { PrintDebug("!B...");$$=!$2; }
|       Translation EQ Translation      { PrintDebug("...=...");$$=BoolEQ($1,$3); $1=$3=NULL; }
|       Translation NE Translation      { PrintDebug("...!=...");$$=!BoolEQ($1,$3); $1=$3=NULL; }
|       Translation _IN Translation     { PrintDebug("..._In...");$$=BoolIn($1,$3); $1=$3=NULL; }
|       Translation GT Translation      { PrintDebug("...>...");$$=BoolGT($1,$3); $1=$3=NULL; }
|       Translation LT Translation      { PrintDebug("...<...");$$=BoolGT($3,$1); $1=$3=NULL; }
|       Translation GE Translation      { PrintDebug("...>=...");$$=BoolGE($1,$3); $1=$3=NULL; }
|       Translation LE Translation      { PrintDebug("...<=...");$$=BoolGE($3,$1); $1=$3=NULL; }
|       EXISTS '(' CD ')'               { PrintDebug("Exists(...)");
                                          $$=Exists($3);
                                          if ($$==2) return 2;
                                          FreeCD($3); $3=NULL; }
|       CD PRECEDES CD                  { PrintDebug("...Precedes...");
                                          $$=Precedes($1,$3);
                                          if ($$==2) return 2;
                                          FreeCD($1); $1=NULL;
                                          FreeCD($3); $3=NULL; }
|       CD FOLLOWS CD                   { PrintDebug("...Follows...");
                                          $$=Precedes($3,$1);
                                          if ($$==2) return 2;
                                          FreeCD($3); $3=NULL;
                                          FreeCD($1); $1=NULL; }
;

Translation :
        '(' Translation ')'             { PrintDebug("(...)");$$=$2; }
|       STRING
|       NUMERIC
|       YEAR                            {
                                          time_t ns;
                                          struct tm *lt;
                                          time(&ns);
                                          lt=localtime(&ns);
                                          $$=AllocTypeInst();
                                          sprintf(tempo,"%04d",1900+lt->tm_year);
                                          $$->str.str(tempo);
                                          $$->val=0;
                                        }
|       MONTH                           {
                                          time_t ns;
                                          struct tm *lt;
                                          time(&ns);
                                          lt=localtime(&ns);
                                          $$=AllocTypeInst();
                                          sprintf(tempo,"%02d",lt->tm_mon+1);
                                          $$->str.str(tempo);
                                          $$->val=0;
                                        }
|       DAY                             {
                                          time_t ns;
                                          struct tm *lt;
                                          time(&ns);
                                          lt=localtime(&ns);
                                          $$=AllocTypeInst();
                                          sprintf(tempo,"%02d",lt->tm_mday);
                                          $$->str.str(tempo);
                                          $$->val=0;
                                        }
|       HOUR                            {
                                          time_t ns;
                                          struct tm *lt;
                                          time(&ns);
                                          lt=localtime(&ns);
                                          $$=AllocTypeInst();
                                          sprintf(tempo,"%02d",lt->tm_hour);
                                          $$->str.str(tempo);
                                          $$->val=0;
                                        }
|       MINUTE                          {
                                          time_t ns;
                                          struct tm *lt;
                                          time(&ns);
                                          lt=localtime(&ns);
                                          $$=AllocTypeInst();
                                          sprintf(tempo,"%02d",lt->tm_min);
                                          $$->str.str(tempo);
                                          $$->val=0;
                                        }
|       SECOND                          {
                                          time_t ns;
                                          struct tm *lt;
                                          time(&ns);
                                          lt=localtime(&ns);
                                          $$=AllocTypeInst();
                                          sprintf(tempo,"%02d",lt->tm_sec);
                                          $$->str.str(tempo);
                                          $$->val=0;
                                        }
|       ORDINAL '(' Translation ')'     {
                                          int i,j;
                                          char tmp1[20];
                                          $$=AllocTypeInst();
                                          sprintf(tmp1,"%d",ordinal);
                                          for (j=0,i=strlen(tmp1);i<$3->val;++i,++j)
                                           tempo[j]='0';
                                          strcpy(&tempo[j],tmp1);
                                          $$->str.str(tempo);
                                          $$->val=0;
                                          FreeTypeInst($3);
                                          $3=NULL;
                                        }
|       NEXTBAL                 {
                                                PrintDebug("NextSub");$$=NextBal();
                                        }
|       PREVIOUSBAL             {
                                                PrintDebug("PreviousSub");$$=PreviousBal();
                                        }
|       VAR_N                           { PrintDebug("N");Copie(&$$,N); }
|       VAR_NT                          { PrintDebug("NT");Copie(&$$,NT); }
|       VAR_NS                          { PrintDebug("NS");Copie(&$$,NS); }
|       VAR_NO                          { PrintDebug("NO");Copie(&$$,NO); }
|       VAR_NTO                         { PrintDebug("NTO"); Copie(&$$,NTO); }
|       VAR_NSO                         { PrintDebug("NSO");Copie(&$$,NSO); }
|       VARS                            { PrintDebug("S");Copie(&$$,S); }
|       VARD                            { PrintDebug("S");Copie(&$$,D); }
|       CD                      { const char *ptr;
                                  PrintDebug("CD");
                                  $$=AllocTypeInst();
                                  ptr=LireCD($1);
                                  if (!ptr) return 2;
                                  $$->str.str(ptr);
                                  $$->val=0;
                                  FreeCD($1);}
|       Translation PLUS Translation    { PrintDebug("...+...");$$=Ajout($1,$3); $1=$3=NULL; }
|       Translation MOINS Translation   { PrintDebug("...-...");$$=Soust($1,$3); $1=$3=NULL; }
|       Translation MULTIPLIE Translation       { PrintDebug("...*...");$$=Multi($1,$3); $1=$3=NULL; }
|       Translation DIVISE Translation  { PrintDebug("...:...");$$=Divis($1,$3); $1=$3=NULL; }
|       STR '(' Translation ')'         { PrintDebug("Str(...)");$$=String($3); }
|       VAL '(' Translation ')'         { PrintDebug("Val(...)");$$=Value($3); }
|       LEN '(' Translation ')'         { PrintDebug("Len(...)");$$=Len($3); }
|       UPPER '(' Translation ')'       { PrintDebug("Upper(...)");$$=Upper($3); }
|       LOWER '(' Translation ')'       { PrintDebug("Lower(...)");$$=Lower($3); }
|       STO '(' Translation ')'         { PrintDebug("Sto(...)");Copie(&$$,S); MemSto($3); $3=NULL; }
|       MEM '(' Translation ')'         { PrintDebug("Mem(...)");$$=MemMem($3); $3=NULL; }
|       EXC '(' Translation ')'         { PrintDebug("Exc(...)");$$=MemExc($3); $3=NULL; }
|       CLR '(' Translation ')'         { PrintDebug("Clr(...)");Copie(&$$,S); MemClr($3); $3=NULL; }
|       FROM '(' Translation ')'        { PrintDebug("From(...)");$$=From($3,0); $3=NULL; }
|       FROM '(' Translation ',' _STRICT ')'
                                        { PrintDebug("From(...,_STRICT)");$$=From($3,1); $3=NULL; }
|       TO '(' Translation ')'          { PrintDebug("To(...)");$$=To($3,0); $3=NULL; }
|       TO '(' Translation ',' _STRICT ')'
                                        { PrintDebug("To(...,_STRICT)");$$=To($3,1); $3=NULL; }
|       BETWEEN '(' Translation ',' Translation ')'
                                        { PrintDebug("Between(...)");$$=Between($3,$5,0); $3=$5=NULL; }
|       BETWEEN '(' Translation ',' Translation ',' _STRICT ')'
                                        { PrintDebug("Between(...,_STRICT)");$$=Between($3,$5,1); $3=$5=NULL; }
|       _DELETE '(' Translation ')'     { PrintDebug("Delete(...)");$$=Replace($3,NULL,0,0); $3=NULL; }
|       _DELETE '(' Translation ',' AT BEGINING ')'
                                        { PrintDebug("Delete(..., AT BEGINING)");$$=Replace($3,NULL,1,0); $3=NULL; }
|       _DELETE '(' Translation ',' AT END ')'
                                        { PrintDebug("Delete(..., AT END)");$$=Replace($3,NULL,2,0); $3=NULL; }
|       _DELETE '(' Translation ',' AT BOTH ')'
                                        { PrintDebug("Delete(...,EVERYWHERE)");$$=Replace($3,NULL,3,0); $3=NULL; }
|       _DELETE '(' Translation ',' _STRICT ')' { PrintDebug("Delete(...)");$$=Replace($3,NULL,0,1); $3=NULL; }
|       _DELETE '(' Translation ',' AT BEGINING ',' _STRICT ')'
                                        { PrintDebug("Delete(..., AT BEGINING)");$$=Replace($3,NULL,1,1); $3=NULL; }
|       _DELETE '(' Translation ',' AT END ',' _STRICT ')'
                                        { PrintDebug("Delete(..., AT END)");$$=Replace($3,NULL,2,1); $3=NULL; }
|       _DELETE '(' Translation ',' AT BOTH ',' _STRICT ')'
                                        { PrintDebug("replace(...,EVERYWHERE)");$$=Replace($3,NULL,3,1); $3=NULL; }
|       REPLACE '(' Translation BY Translation ')'      { PrintDebug("replace(...)");$$=Replace($3,$5,0,0); $3=$5=NULL; }
|       REPLACE '(' Translation BY Translation ',' AT BEGINING ')'
                                        { PrintDebug("replace(..., AT BEGINING)");$$=Replace($3,$5,1,0); $3=$5=NULL; }
|       REPLACE '(' Translation BY Translation ',' AT END ')'
                                        { PrintDebug("replace(..., AT END)");$$=Replace($3,$5,2,0); $3=$5=NULL; }
|       REPLACE '(' Translation BY Translation ',' AT BOTH ')'
                                        { PrintDebug("replace(...,EVERYWHERE)");$$=Replace($3,$5,3,0); $3=$5=NULL; }
|       REPLACE '(' Translation BY Translation ',' _STRICT ')'
                                        { PrintDebug("replace(...)");$$=Replace($3,$5,0,1); $3=$5=NULL; }
|       REPLACE '(' Translation BY Translation ',' AT BEGINING ',' _STRICT ')'
                                        { PrintDebug("replace(..., AT BEGINING)");$$=Replace($3,$5,1,1); $3=$5=NULL; }
|       REPLACE '(' Translation BY Translation ',' AT END ',' _STRICT ')'
                                        { PrintDebug("replace(..., AT END)");$$=Replace($3,$5,2,1); $3=$5=NULL; }
|       REPLACE '(' Translation BY Translation ',' AT BOTH ',' _STRICT ')'
                                        { PrintDebug("replace(...,EVERYWHERE)");$$=Replace($3,$5,3,1); $3=$5=NULL; }
|       REPLACEOCC '(' Translation BY Translation ',' Translation ')'
                                        { PrintDebug("replace(...,Occ)");$$=ReplaceOcc($3,$5,$7,0); $3=$5=$7=NULL; }
|       REPLACEOCC '(' Translation BY Translation ',' Translation ',' _STRICT ')'
                                        { PrintDebug("replace(...,Occ)");$$=ReplaceOcc($3,$5,$7,1); $3=$5=$7=NULL; }
|       BFIRST '(' Translation ')'      { PrintDebug("BFirst(...)");$$=BFirst($3,0); $3=NULL; }
|       BFIRST '(' WNUMBER ')'          { PrintDebug("BFirst(Number)");$$=BFirst(NULL,1); }
|       BFIRST '(' WSTRING ')'          { PrintDebug("BFirst(String)");$$=BFirst(NULL,2); }
|       EFIRST '(' Translation ')'      { PrintDebug("EFirst(...)");$$=EFirst($3,0); $3=NULL; }
|       EFIRST '(' WNUMBER ')'          { PrintDebug("EFirst(Number)");$$=EFirst(NULL,1); }
|       EFIRST '(' WSTRING ')'          { PrintDebug("EFirst(String)");$$=EFirst(NULL,2); }
|       BLAST '(' Translation ')'       { PrintDebug("BLast(...)");$$=BLast($3,0); $3=NULL; }
|       BLAST '(' WNUMBER ')'           { PrintDebug("BLast(Number)");$$=BLast(NULL,1); }
|       BLAST '(' WSTRING ')'           { PrintDebug("BLast(String)");$$=BLast(NULL,2); }
|       ELAST '(' Translation ')'       { PrintDebug("ELast(...)");$$=ELast($3,0); $3=NULL; }
|       ELAST '(' WNUMBER ')'           { PrintDebug("ELast(Number)");$$=ELast(NULL,1); }
|       ELAST '(' WSTRING ')'           { PrintDebug("ELast(String)");$$=ELast(NULL,2); }
|       NEXT '(' CD ')'                 { PrintDebug("Next(...)");$$=Next_($3,NULL,0); $3=NULL; }
|       NEXT '(' CD ',' CD ')'          { PrintDebug("Next(...,...)");
                                          $$=Next_($3,$5,0);
                                          if (!$$) return 2;
                                          $3=$5=NULL; }
|       NEXT '(' CD ',' _STRICT ')'     { PrintDebug("Next(...,_STRICT)");
                                          $$=Next_($3,NULL,1);
                                          if (!$$) return 2;
                                          $3=NULL; }
|       NEXT '(' CD ',' CD ',' _STRICT ')'
                                        { PrintDebug("Next(...,...,_STRICT)");
                                          $$=Next_($3,$5,1);
                                          if (!$$) return 2;
                                          $3=$5=NULL; }
|       LAST '(' CD ')'                 { PrintDebug("Last(...)");
                                          $$=Last_($3,NULL,0);
                                          if (!$$) return 2;
                                          $3=NULL; }
|       LAST '(' CD ',' CD ')'          { PrintDebug("Last(...,...)");
                                          $$=Last_($3,$5,0);
                                          if (!$$) return 2;
                                          $3=$5=NULL; }
|       LAST '(' CD ',' _STRICT ')'     { PrintDebug("Last(...,_STRICT)");
                                          $$=Last_($3,NULL,1);
                                          if (!$$) return 2;
                                          $3=NULL; }
|       LAST '(' CD ',' CD ',' _STRICT ')'
                                        { PrintDebug("Last(...,...,_STRICT)");
                                          $$=Last_($3,$5,1);
                                          if (!$$) return 2;
                                          $3=$5=NULL; }
|       SORT '(' STRING ')'     { PrintDebug("Sort");
                                  MustSort($3->str.str());
                                  FreeTypeInst($3);
                                  $3=NULL;
                                  return 2;
                                }
|       TABLE '(' STRING ')'            { PrintDebug("Table(...)");$$=Table_($3); $3=NULL; }
|       REGFIND '(' Translation ')'     { PrintDebug("RegFind(...)");$$=RegFind($3, NULL); $3=NULL; }
|       REGFIND '(' Translation ',' Translation ')'     { PrintDebug("RegFind(...,...)");$$=RegFind($3,$5); $3=$5=NULL; }
|       REGMATCH '(' Translation ')'    { PrintDebug("RegMatch(...)");$$=RegMatch($3); $3=NULL; }
|       REGREPLACE '(' Translation ',' Translation ')'
                                        { PrintDebug("RegReplace(...,...)");$$=RegReplace($3,$5,NULL); $3=$5=NULL; }
|       REGREPLACE '(' Translation ',' Translation ',' Translation ')'
                                        { PrintDebug("RegReplace(...,...,...)");$$=RegReplace($3,$5,$7); $3=$5=$7=NULL; }
;

%%



