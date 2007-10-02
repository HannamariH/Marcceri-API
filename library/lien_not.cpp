/*
 *  USEMARCON Software - Command Line version
 *  Copyright The British Library, The USEMarcon Consortium, 1995-2000
 *  Adapted by Crossnet Systems Limited - British Library Contract No. BSDS 851
 *
 *  Adapted by ATP Library Systems Ltd, Finland, 2002-2003
 *  Adapted by The National Library of Finland, 2004-2007
 *
 *  File:  lien_not.cpp
 *
 *  a bunch of functions to manage strings and interface to the
 *  lexical analysis
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "tcdlib.h"
#include "sort.h"
#include "umrecord.h"
#include "trule.h"
#include "codedata.h"
#include "lien_not.h"
#include "truledoc.h"
#include "tools.h"
#include "rulefile.h"
#include "pcre.h"

#define C_NEW -20;
#define C_NEWEST -21;

int TMarcScannerImpl::LexerInput(char *buf, int max_size)
{
    if (itsBufferPos >= itsBufferLen)
    {
        buf = NULL;
        return 0;
    }
    long num = itsBufferLen - itsBufferPos;
    if (num > max_size)
        num = max_size;
    memcpy(buf, &itsBuffer[itsBufferPos], num);
    itsBufferPos += num;

    return num;
}

void TMarcScannerImpl::SetRule(TRule *Rule)
{
    itsBufferPos = 0;

    mLine = Rule->GetLineNo();

    char *lib = Rule->GetLib();
    if (lib)
        itsBufferLen = strlen(lib) + 3;
    else
        itsBufferLen = 4;

    if (itsBuffer)
        free(itsBuffer);
    itsBuffer = (char *) malloc(itsBufferLen+1);

    if (lib)
        sprintf(itsBuffer, "%s\n#", lib);
    else
        sprintf(itsBuffer, "S\n#");

    yyrestart(NULL);
}

void TMarcScannerImpl::RewindBuffer()
{
    itsBufferPos = 0;

    yyrestart(NULL);
}

// Cette fonction est appellee par l'analyseur grammatical en cas d'erreur

int TEvaluateRule::yylex()
{
    bool ok = true;
    int token=itsScanner.yylex(yylval, ok, &m_allocator);
    if (!ok)
        yyerror("lex error");

    return token;
}

void TEvaluateRule::yyerror( char *m )
{
    typestr errorstr; 
    if (*m=='s' || *m=='l')
    {
        errorstr = m;
        errorstr += " near ";
        errorstr += itsScanner.YYText();
    }
    else
    {
        errorstr = "ERROR '";
        errorstr += m;
    }
    errorstr += " in rule \"";
    ++Error;
    typestr rulestr;
    CurrentRule->ToString(rulestr);
    errorstr += itsScanner.getCurrentLineContents();
    if (errorstr.str()[strlen(errorstr.str())-2] == '\n')
        errorstr.str()[strlen(errorstr.str())-2] = '\0';
    if (itsScanner.getCurrentLineNo() > 0)
    {
        errorstr += "\" at line ";
        char lineno[30];
        sprintf(lineno, "%d", itsScanner.getCurrentLineNo());
        errorstr += lineno;
    }
    itsErrorHandler->SetErrorD(5100, ERROR, errorstr.str());
}

// Cette fonction permet de savoir si une regle commence par un +

int TEvaluateRule::IsConcat(char* lib)
{
    if (lib==NULL) return 0;
    char*ptr=lib;
    while(*ptr && *ptr==' ') ++ ptr;
    if (*ptr=='+') return 1; else return 0;
}


// Cette fonction remplace les valeurs symboliques de n,ns et nt par leur
// valeur reelle

int TEvaluateRule::Replace_N_NT_NS( int val, int N, int NT, int NS )
{
    switch(val)
    {
    case CD_N  : return N;
    case CD_NT : return NT;
    case CD_NS : return NS;
    case CD_NEW : return C_NEW;
    case CD_NEWEST : return C_NEWEST;
    default    : return val;
    }
}


void TEvaluateRule::ResetRedo()
{
    AfterRedo=RedoFlag=false;
    if (RedoStr) free(RedoStr);
    RedoStr=NULL;
    if (MainInput) free(MainInput);
    MainInput=NULL;
}


int TEvaluateRule::Init_Evaluate_Rule(void *Doc, TRuleDoc *RDoc, TError *ErrorHandler,
                                      int dbg_rule, unsigned long ord, bool UTF8Mode)
{
    debug_rule = dbg_rule;
#if YY_MarcParser_DEBUG != 0
    YY_MarcParser_DEBUG_FLAG = debug_rule;
#endif
    ordinal = ord;
    RuleDoc=RDoc;
    itsErrorHandler = ErrorHandler;
    itsUTF8Mode = UTF8Mode;
    int i;
    S=AllocTypeInst();
    D=AllocTypeInst();
    N=AllocTypeInst();
    NT=AllocTypeInst();
    NS=AllocTypeInst();
    NO=AllocTypeInst();
    NTO=AllocTypeInst();
    NSO=AllocTypeInst();
    NEW=AllocTypeInst();
    NEWEST=AllocTypeInst();
    CDIn=AllocCD();
    if (!S ||!D ||!N || !NT || !NS || !NO || !NSO || !NTO || !NEW || !NEWEST || !CDIn)
        itsErrorHandler->SetErrorD(5000,ERROR,"When initialising variables");

    NEW->val = C_NEW;
    NEWEST->val = C_NEWEST;

    // -------------------------------------------------
    // Initialisation du fichier de communication des regles :
    // pour evaluer les regles par lex&yacc, il faut les ecrire dans
    // un fichier. Le fichier suivant est ouvert avec une taille de bloc
    // normalement suffisante pour que la regle ne soit jamais physiquement
    // flushe sur disque

    // On initialise les memoires. Elles ne sont pas remises a zero entre chaque
    // notice, ce qui permet de recuperer des infos des notices precedantes par exemple.
    for (i=0;i<100;++i)
    {
        Memoire[i]=AllocTypeInst();
        if (!Memoire[i])
            itsErrorHandler->SetErrorD(5000,ERROR,"When initialising memories");
    }

    return 0;
}

const char* TEvaluateRule::find_sep(const char *a_str)
{
    bool in_quotes = false;
    int block = 0;
    const char* p = a_str;
    while (*p)
    {
        if (*p == '\'')
            in_quotes = !in_quotes;
        if (!in_quotes)
        {
            switch (*p)
            {
            case ';':
                if (block == 0)
                    return p;
                break;
            case '{':
                ++block;
                break;
            case '}':
                --block;
                break;
            }
        }
        ++p;
    }
    return p;
}

const char* TEvaluateRule::find_statement_start(const char *a_str)
{
    const char* p = a_str;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
    {
        ++p;
    }
    if ((*p == 'E' || *p == 'e') && strncmp(p + 1, "lse ", 4) == 0)
        p += 5;
    return p;
}

const char* TEvaluateRule::find_statement_end(const char *a_str)
{
    bool in_quotes = false;
    int block = 0;
    int paren = 0;
    const char* p = a_str;
    while (*p)
    {
        if (*p == '\'')
            in_quotes = !in_quotes;
        if (!in_quotes)
        {
            switch (*p)
            {
            case ';':
                if (block == 0 && paren == 0)
                    return p;
                break;
            case '{':
                if (paren == 0)
                    ++block;
                break;
            case '}':
                if (paren == 0)
                    --block;
                break;
            case '(':
                ++paren;
                break;
            case ')':
                --paren;
                if (paren == 0 && block == 0)
                {
                    // Try to be compatible with old quirky syntax where 
                    // 'If (something) and/or (something) Then ...' is legal
                    const char *p2 = find_statement_start(p + 1);
                    if (!(((*p2 == 'A' || *p2 == 'a') && *(p2 + 1) == 'n' && *(p2 + 2) == 'd')
                      || ((*p2 == 'O' || *p2 == 'o') && *(p2 + 1) == 'r')))
                        return p + 1;
                }
                break;
            case 't':
            case 'T':
                if (block == 0 && strncmp(p + 1, "hen ", 4) == 0)
                    return p;
                break;
            case 'e':
            case 'E':
                if (block == 0 && strncmp(p + 1, "lse ", 4) == 0)
                    return p;
                break;
            }
        }
        ++p;
    }
    return p;
}

int TEvaluateRule::InnerParse(TRule* a_rule, const char *a_rulestr)
{
    // Parse statements one by one handling conditionals separately 
    // until If - Then returns 2.
    typestr statement;
    const char *rulep = a_rulestr;
    int rc = 0;
    while (true)
    {
        if (!statement.is_empty())
        {
            char* stmt = statement.str();
            if (((*stmt == 'I' || *stmt == 'i') && *(stmt + 1) == 'f' && (*(stmt + 2) == ' ' || *(stmt + 2) == '('))
              || ((*stmt == 'W' || *stmt == 'w') && *(stmt + 1) == 'h' && *(stmt + 2) == 'i' && *(stmt + 3) == 'l' && *(stmt + 4) == 'e' && (*(stmt + 5) == ' ' || *(stmt + 5) == '(')))
            {
                bool while_loop = *(stmt + 1) == 'h';
                const char* p_if = find_statement_end(stmt);
                typestr check_stmt = "Check";
                check_stmt.append(stmt + 2, p_if - stmt - 2);

                TRule rule(a_rule, check_stmt.str());
                itsScanner.SetRule(&rule);
                rc = yyparse();
                bool then_found;
                p_if = find_statement_start(p_if);
                if ((*p_if == 'T' || *p_if == 't') && strncmp(p_if + 1, "hen ", 4) == 0)
                {
                    then_found = true;
                    p_if += 5;
                }
                else
                {
                    then_found = false;
                }

                p_if = find_statement_start(p_if);
                const char* p_stmt_true = find_statement_end(p_if);
                if (rc == 4)
                {
                    statement.str(p_if, p_stmt_true - p_if);
                }
                else
                {
                    p_stmt_true = find_statement_start(p_stmt_true);
                    const char* p_stmt_else = find_statement_end(p_stmt_true);
                    statement.str(p_stmt_true, p_stmt_else - p_stmt_true);
                    if (!statement.is_empty() || !then_found)
                        rc = 0;
                }
                InnerParse(a_rule, statement.str());
                int loop_counter = 0;
                while (while_loop && rc == 4)
                {
                    itsScanner.RewindBuffer();
                    rc = yyparse();
                    if (rc == 4)
                        InnerParse(a_rule, statement.str());
                    if (++loop_counter >= 1000)
                    {
                        itsErrorHandler->SetError(5002, WARNING);
                        break;
                    }
                }
                statement = "";
                continue;
            }
            else if ((*stmt == 'F' || *stmt == 'f') && *(stmt + 1) == 'o' && *(stmt + 2) == 'r' && (*(stmt + 3) == ' ' || *(stmt + 3) == '('))
            {
                const char* p_for = find_statement_end(stmt);
                typestr params;
                params.str(stmt + 4, p_for - stmt - 4);

                RegExp re("[\\s(]*(.*?)\\s+[Ff]rom\\s+(\\d+)\\s+[tT]o\\s+(\\d+)(\\s+Condition\\s*\\((.*)\\))?", false);
                int res = re.exec(params.str());
                if (res >= 3)
                {
                    typestr variable, from_str, to_str, condition;
                    re.match(1, variable);
                    re.match(2, from_str);
                    re.match(3, to_str);
                    re.match(5, condition);
                    int from = atoi(from_str.str());
                    int to = atoi(to_str.str());
                    int step = (from <= to ? 1 : -1);
                    re.init("^\\s*{+\\s*(.*)\\s*}\\s*$", false);
                    for (int i = from; (step < 0 && i >= to) || (step > 0 && i <= to); i += step)
                    {
                        if (!condition.is_empty())
                        {
                            typestr check_stmt = "Check (";
                            check_stmt.append(condition);
                            check_stmt.append(")");

                            TRule rule(a_rule, check_stmt.str());
                            itsScanner.SetRule(&rule);
                            rc = yyparse();
                            if (rc != 4)
                                break;
                        }

                        typestr for_statement;
                        if (re.exec(p_for) > 0)
                            re.match(1, for_statement);
                        else
                            for_statement = p_for;
                        
                        char i_str[30];
                        sprintf(i_str, "%ld", i);
                        for_statement.replace(variable.str(), i_str);
                        InnerParse(a_rule, for_statement.str());
                    }
                }
                else
                {
                    itsErrorHandler->SetErrorD(5100, ERROR, statement.str());
                    rc = 2;
                }
                statement = "";
                continue;
            }

            TRule rule(a_rule, statement.str());
            itsScanner.SetRule(&rule);
            rc = yyparse();
        }            

        if (rc == 2 || !rulep)
            break;
        while (*rulep == ' ' || *rulep == '\t' || *rulep == '\n' || *rulep == '\r')
            ++rulep;
        const char* p = find_sep(rulep);
        if (p)
        {
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *rulep == ';')
                ++p;
            statement.str(rulep, p - rulep);
            if (*p) 
                rulep = p + 1;
            else
                rulep = NULL;
        }
        else
        {
            statement = rulep;
            rulep = NULL;
        }
    }
    return rc;
}

int TEvaluateRule::Parse(TRule* a_rule)
{
    char *rulestr = a_rule->GetLib();
    int rc = 0;
    if (mParserIfRegExp.exec(rulestr) >= 1)
    {
        // The rule contains conditionals
        rc = InnerParse(a_rule, rulestr);
    }
    else
    {
        itsScanner.SetRule(a_rule);
        rc = yyparse();
    }
    return rc;
}

//
// Cette fonction effectue la conversion d'un UMRecord en un autre, conformement
// a une regle.
//

int TEvaluateRule::Evaluate_Rule( TUMRecord* In, TUMRecord* Out, TUMRecord* RealOut, TRule* Rule, TCDLib* ProcessCDL /* = NULL */ )
{
    // -------------------------------------------------
    // Renseignement des variables globales
    InputRecord = In;
    OutputRecord = Out;
    RealOutputRecord = RealOut;
    CurrentRule = Rule;

    // -------------------------------------------------
    // On parcourt tous les CDLib qui correspondent au CD en entree de la regle
    TCDLib* aCDLIn;
    if (ProcessCDL) 
        aCDLIn = NULL;
    else
        aCDLIn = In->GetFirstCDLib();

    if (debug_rule)
    {
        typestr tmp;
        Rule->ToString(tmp);
        printf("\nDebug: Processing rule: '%s'\n", tmp.str());
    }

    while(true)
    {
        // Make sure that if ProcessCDL is set we process it once and break out when done
        if (ProcessCDL)
        {
            if (aCDLIn)
                break;
            aCDLIn = ProcessCDL;
        }
        else
        {
            if (!In->NextCD(&aCDLIn, Rule->GetInputCD()))
                break;
        }

        // Initialisation de la memoire pour les variables de l'analyse de regles
        if (!S)
        {
            S=AllocTypeInst();
        }
        if (!D)
        {
            D=AllocTypeInst();
        }
        if (!S ||!D ||!N || !NT || !NS || !NO || !NSO || !NTO || !CDIn || !NEW)
            itsErrorHandler->SetErrorD(5000,ERROR,"When initialising variables");

        // Initialisation de S,N,NT,NS : valeurs de l'entree
        N->str.freestr();
        NT->str.freestr();
        NS->str.freestr();
        N->val=aCDLIn->GetOccurrenceNumber();
        NT->val=aCDLIn->GetTagOccurrenceNumber();
        if (*(Rule->GetInputCD()->GetSubfield()))
            NS->val=aCDLIn->GetSubOccurrenceNumber();
        else
            NS->val=0;
        S->val=0;
        TCD inCD = Rule->GetInputCD();
        inCD.ReplaceWildcards(aCDLIn->GetTag(), aCDLIn->GetSubfield());
        S->str = aCDLIn->GetContent(&inCD);
        InputCDL=aCDLIn;

        if (debug_rule)
        {
            printf("Debug: Input field: %s%s o:%d t-o:%d s-o:%d: '%s'\n", aCDLIn->GetTag(), aCDLIn->GetSubfield() ? aCDLIn->GetSubfield() : "", 
                aCDLIn->GetOccurrenceNumber(), aCDLIn->GetTagOccurrenceNumber(), aCDLIn->GetSubOccurrenceNumber(), S->str.str());
        }

        // Initialisation des parametres du CDIn
        strcpy(CDIn->Field, aCDLIn->GetTag());
        strcpy(CDIn->SubField, aCDLIn->GetSubfield());
        CDIn->nt=aCDLIn->GetTagOccurrenceNumber();
        CDIn->ns=aCDLIn->GetSubOccurrenceNumber();


        // -------------------------------------------------
        // Boucle d'evaluation de la regle
        int rc;
        do
        {
            // On cree une copie du CDOut (on va peut-etre le modifier)
            TCD* aCDOut=new TCD(Rule->GetOutputCD());

            aCDOut->ReplaceWildcards(CDIn->Field, CDIn->SubField);

            // Si n, ns ou nt apparaissent dans CDOut, on les remplace par leur valeur
            aCDOut->SetOccurrenceNumber(
                Replace_N_NT_NS(aCDOut->GetOccurrenceNumber(), N->val, NT->val, NS->val) );
            aCDOut->SetTagOccurrenceNumber(
                Replace_N_NT_NS(aCDOut->GetTagOccurrenceNumber(), N->val, NT->val, NS->val) );
            if (*(aCDOut->GetSubfield())=='I')
                aCDOut->SetSubOccurrenceNumber(1);
            else
                aCDOut->SetSubOccurrenceNumber(
                    Replace_N_NT_NS(aCDOut->GetSubOccurrenceNumber(), N->val, NT->val, NS->val) );

            NO->str.freestr();
            NTO->str.freestr();
            NSO->str.freestr();

            if (D) FreeTypeInst(D);
            D=AllocTypeInst();

            TCDLib* aCDLOut=Out->GetLastCDLib();

            bool concatenation = false;

            if (aCDOut->GetTagOccurrenceNumber() == CD_NEWEST)
            {
                // Find the correct occurrence number for the newest tag
                TCD findCD(itsErrorHandler);
                findCD.SetTag(aCDOut->GetTag());
                if (Out->PreviousCD(&aCDLOut, &findCD))
                    aCDOut->SetTagOccurrenceNumber(aCDLOut->GetTagOccurrenceNumber());
                aCDLOut=Out->GetLastCDLib();
            }
            
            // Find previous field from CDLib and initialize value of D from it
            // Note: This is fragile code. There are old conversions that rely on the quirks
            // such as adding a new field by specifying xxx(nto) as the target.
            if (aCDOut->GetTagOccurrenceNumber() != CD_NEW && Out->PreviousCD(&aCDLOut, aCDOut))
            {
                // Previous field exists. We have two possible cases:
                // If a subfield exists, NO, NSO and NTO are set from this field.
                // If not, we are at the end of the field and it doesn't have
                // subfields, therefore NSO=0 and NO=NTO

                D->val=0;
                if (*(aCDOut->GetSubfield()))
                {
                    NO->val=aCDLOut->GetOccurrenceNumber();
                    NTO->val=aCDLOut->GetTagOccurrenceNumber();
                    NSO->val=aCDLOut->GetSubOccurrenceNumber();
                }
                else
                {
                    NSO->val=0;
                    NO->val=NTO->val=aCDLOut->GetTagOccurrenceNumber();

                    // Position at the beginning of the CDLib representing a complete field
                    TCDLib* Courant=(TCDLib*)aCDLOut->GetPrevious();
                    TCD ToSearch(itsErrorHandler);
                    ToSearch.SetTag(aCDLOut->GetTag());
                    ToSearch.SetTagOccurrenceNumber(aCDLOut->GetTagOccurrenceNumber());
                    while( Courant )
                    {
                        if (*Courant==ToSearch ) Courant=(TCDLib*)Courant->GetPrevious();
                        else break;
                    }
                    if (Courant)
                        aCDLOut=(TCDLib*)Courant->GetNext();
                    else
                        aCDLOut=Out->GetFirstCDLib();
                }

                // If the rule does not contain + at the beginning, we need to check
                // if NO, NTO or NSO should be incremented
                if (!IsConcat(Rule->GetLib()))
                {
                    // Different cases:
                    // TTT(no)SS
                    if (aCDOut->GetTagOccurrenceNumber()==CD_NO)
                    {
                        if (aCDOut->GetSubOccurrenceNumber()<0)
                        {
                            typestr tmp;
                            Rule->ToString(tmp);
                            itsErrorHandler->SetErrorD(5101, ERROR, tmp.str());
                        }
                        NTO->val=++NO->val;

                        D->str.str("");
                    }
                    else if (aCDOut->GetSubOccurrenceNumber()==CD_NO)  // TTTSS(no)
                    {
                        if (aCDOut->GetTagOccurrenceNumber()<=0)
                        {
                            typestr tmp;
                            Rule->ToString(tmp);
                            itsErrorHandler->SetErrorD(5102, ERROR, tmp.str());
                        }
                        NSO->val=++NO->val;
                        D->str.str("");
                    }
                    else if (aCDOut->GetTagOccurrenceNumber()==CD_NTO) // TTT(nto)SS
                    {
                        if (aCDOut->GetSubOccurrenceNumber()<0)
                        {
                            typestr tmp;
                            Rule->ToString(tmp);
                            itsErrorHandler->SetErrorD(5103, ERROR, tmp.str());
                        }
                        ++NTO->val; ++NO->val;

                        D->str.str("");
                    }
                    else if (aCDOut->GetSubOccurrenceNumber()==CD_NSO) // TTTSS(nso)
                    {
                        if (aCDOut->GetTagOccurrenceNumber()<=0)
                        {
                            typestr tmp;
                            Rule->ToString(tmp);
                            itsErrorHandler->SetErrorD(5104, ERROR, tmp.str());
                        }
                        ++NSO->val; ++NO->val;
                        D->str.str("");
                    }
                    else
                    {
                        concatenation = true;
                        D->str = aCDLOut->GetContent(aCDOut);
                    }
                }
                else
                {
                    concatenation = true;
                    D->str = aCDLOut->GetContent(aCDOut);
                }
            }
            else
            {
                // No corresponding CDLib found
                D->val=0;
                D->str.str("");

                NO->val=aCDOut->GetOccurrenceNumber();
                NSO->val=aCDOut->GetSubOccurrenceNumber();
                NTO->val=aCDOut->GetTagOccurrenceNumber();

                // Find values for no, nso and nto.
                // Different cases:
                // TTT(no)SS
                if (aCDOut->GetTagOccurrenceNumber()==CD_NO)
                {
                    if (aCDOut->GetSubOccurrenceNumber()<0)
                    {
                        typestr tmp;
                        Rule->ToString(tmp);
                        itsErrorHandler->SetErrorD(5101, ERROR, tmp.str());
                    }
                    NTO->val=NO->val=1;
                }
                else
                {
                    // TTTSS(no)
                    if (aCDOut->GetSubOccurrenceNumber()==CD_NO)
                    {
                        if (aCDOut->GetTagOccurrenceNumber()<=0)
                        {
                            typestr tmp;
                            Rule->ToString(tmp);
                            itsErrorHandler->SetErrorD(5102, ERROR, tmp.str());
                        }
                        NSO->val=NO->val=1;
                    }
                    else
                    {
                        // TTT(nto)SS
                        if (aCDOut->GetTagOccurrenceNumber()==CD_NTO)
                        {
                            if (aCDOut->GetSubOccurrenceNumber()<0)
                            {
                                typestr tmp;
                                Rule->ToString(tmp);
                                itsErrorHandler->SetErrorD(5103, ERROR, tmp.str());
                            }
                            NTO->val=1;
                        }
                        else
                        {
                            // TTTSS(nso)
                            if (aCDOut->GetSubOccurrenceNumber()==CD_NSO)
                            {
                                if (aCDOut->GetTagOccurrenceNumber()<=0)
                                {
                                    typestr tmp;
                                    Rule->ToString(tmp);
                                    itsErrorHandler->SetErrorD(5104, ERROR, tmp.str());
                                }
                                NSO->val=1;
                            }
                        }
                    }
                }
            }

            if (debug_rule && !D->str.is_empty())
            {
                printf("Debug: Existing output field: '%s'\n", D->str.str());
            }

            aCDOut->SetTagOccurrenceNumber(NTO->val);
            aCDOut->SetSubOccurrenceNumber(NSO->val);

            // If NO is not set, find the correct value for it
            if (NO->val<=0)
            {
                // Find last matching CD
                TCDLib* Search=Out->GetLastCDLib();
                TCD aCD(aCDOut);
                aCD.SetTagOccurrenceNumber(0);
                aCD.SetSubOccurrenceNumber(0);

                if (Out->PreviousCD(&Search,&aCD))
                {
                    if (*(Search->GetSubfield())=='$')
                        NO->val=Search->GetOccurrenceNumber()+1;
                    else
                        NO->val=0;
                }
                else
                    NO->val=1;
            }
            aCDOut->SetOccurrenceNumber(NO->val);

            Error=0;
            RedoFlag=false;
            mCDOut=aCDOut;
            rc = Parse(Rule);

            if (rc!=2)
            {
                TCDLib aCDLOut(aCDOut);
                aCDLOut.SetBeginning(0);
                aCDLOut.SetEnd(0);

                aCDLOut.SetContent(ToString(S));

                Out->InsertCDLib(&aCDLOut, aCDOut, concatenation);

                if (debug_rule)
                {
                    if (In == Out)
                        printf("Debug: Setting input field: ");
                    else
                        printf("Debug: Setting output field: ");
                    printf("%s%s o:%d t-o:%d s-o:%d: '%s'\n", aCDOut->GetTag(), aCDOut->GetSubfield() ? aCDOut->GetSubfield() : "", aCDOut->GetOccurrenceNumber(), aCDOut->GetTagOccurrenceNumber(), aCDOut->GetSubOccurrenceNumber(), S->str.str());
                }
            }
            if (T)
            {
                FreeTypeInst(S);
                S=T;
                T=NULL;
            }
            delete aCDOut;
        }
        while( rc==3 );

        if (!ProcessCDL)
        {
            // Move to the next field in input
            TCD Courant(Rule->GetInputCD());
            if (Courant.TagContainsWildcard())
                Courant.SetTag(CDIn->Field);
            if (Courant.SubfieldContainsWildcard())
                Courant.SetSubfield(CDIn->SubField);
            
            Courant.SetTagOccurrenceNumber(NT->val);
            Courant.SetSubOccurrenceNumber(NS->val);

            while (aCDLIn)
            {
                if (*aCDLIn!=Courant) break;
                aCDLIn=(TCDLib*)aCDLIn->GetNext();
            }
        }
    }

    return itsErrorHandler->GetErrorCode();
}


int TEvaluateRule::End_Evaluate_Rule()
{
    int i;
    // Liberation de la memoire
    for (i=0;i<100;++i)
        if (Memoire[i])
        {
            FreeTypeInst(Memoire[i]);
            Memoire[i]=NULL;
        }

        if (S) FreeTypeInst(S); S=NULL;
        if (T) FreeTypeInst(T); T=NULL;

        if (D) FreeTypeInst(D); D=NULL;

        if (N) FreeTypeInst(N); N=NULL;
        if (NT) FreeTypeInst(NT); NT=NULL;
        if (NS) FreeTypeInst(NS); NS=NULL;
        if (NO) FreeTypeInst(NO); NO=NULL;
        if (NTO) FreeTypeInst(NTO); NTO=NULL;
        if (NSO) FreeTypeInst(NSO); NSO=NULL;
        if (NEW) FreeTypeInst(NEW); NEW=NULL;
        if (NEWEST) FreeTypeInst(NEWEST); NEWEST=NULL;
        FreeCD(CDIn); CDIn=NULL;

        return 0;
}

void TEvaluateRule::FinishCD(TypeCD* aCD)
{
    if (!aCD)
        return;
    switch(aCD->nt)
    {
    case CD_N   : aCD->nt=N->val; break;
    case CD_NS  : aCD->nt=NS->val; break;
    case 0 :
    case CD_NT  : aCD->nt=NT->val; break;
    case CD_NO  : aCD->nt=NO->val; break;
    case CD_NSO : aCD->nt=NSO->val; break;
    case CD_NTO : aCD->nt=NTO->val; break;
    }
    switch(aCD->ns)
    {
    case CD_N   : aCD->ns=N->val; break;
    case CD_NS  : aCD->ns=NS->val; break;
    case CD_NT  : aCD->ns=NT->val; break;
    case CD_NO  : aCD->ns=NO->val; break;
    case CD_NSO : aCD->ns=NSO->val; break;
    case CD_NTO : aCD->ns=NTO->val; break;
    }
}

void TEvaluateRule::FinishTCD(TypeCD* aCD)
{
    if (!aCD)
        return;
    switch(aCD->nt)
    {
    case CD_N   : aCD->nt=N->val; break;
    case CD_NS  : aCD->nt=NS->val; break;
    case 0 :
    case CD_NT  : aCD->nt=NT->val; break;
    case CD_NO  : aCD->nt=NO->val; break;
    case CD_NSO : aCD->nt=NSO->val; break;
    case CD_NTO : aCD->nt=NTO->val; break;
    }
}

void TEvaluateRule::PrCD( TypeCD* CD )
{
    if (CD)
        printf("%s(%d)%s(%d)",CD->Field,CD->nt,CD->SubField,CD->ns);
}

// Return the contents of the CD
typestr TEvaluateRule::ReadCD(TypeCD *CD)
{
    FinishCD(CD);

    TUMRecord *rec = CD->Output ? RealOutputRecord : InputRecord;
    TCDLib *aCDL=rec->GetFirstCDLib();
    TCD     aCD(CD, itsErrorHandler);
    typestr ptr;

    if (rec->NextCD(&aCDL,&aCD))
        ptr = aCDL->GetContent(&aCD);
    else
        ptr.freestr();
    return ptr;
}

int TEvaluateRule::Exists( TypeCD* CD ) 
{
    FinishTCD(CD);

    TUMRecord *rec = CD->Output ? RealOutputRecord : InputRecord;
    TCDLib *aCDL=rec->GetFirstCDLib();
    TCD     aCD(CD, itsErrorHandler);

    return rec->NextCD(&aCDL,&aCD);
}

int TEvaluateRule::ExistsIn(TypeInst* a_str, TypeCD* a_cd)
{
    FinishTCD(a_cd);
    char subfield[3];
    subfield[0] = START_OF_FIELD;
    subfield[1] = a_cd->SubField[1];
    subfield[2] = '\0';
    char* p = strstr(a_str->str.str(), subfield);
    int occurrence = 0;
    while (p)
    {
        ++occurrence;
        if (a_cd->ns == 0 || a_cd->ns == occurrence)
        {
            return 1;
        }
    }
    return 0;
}

int TEvaluateRule::Precedes( TypeCD* CD1, TypeCD* CD2 ) 
{
    FinishCD(CD1);
    FinishCD(CD2);

    TUMRecord *rec = CD1->Output ? RealOutputRecord : InputRecord;
    TCDLib* aCDL=rec->GetFirstCDLib();
    TCD     aCD1(CD1, itsErrorHandler);
    TCD     aCD2(CD2, itsErrorHandler);

    if (!rec->NextCD(&aCDL,&aCD1)) return 0;
    aCDL = (TCDLib *) aCDL->GetNext();
    if (!rec->NextCD(&aCDL,&aCD2)) return 0;
    else return 1;
}

typestr TEvaluateRule::NextSubField( TypeCD* CD1, TypeCD* CD2 ) // Idem
{
    FinishCD(CD1);
    FinishCD(CD2);

    TCDLib* aCDL = InputCDL;
    TCD     aCD1(CD1, itsErrorHandler);
    typestr temps = "";

    if (aCDL) aCDL=(TCDLib*)aCDL->GetNext();
    if (CD2==NULL)
    {
        if (InputRecord->NextCD(&aCDL,&aCD1))
            temps = aCDL->GetContent();
    }
    else
    {
        TCD   aCD2(CD2, itsErrorHandler);
        while(aCDL)
        {
            if (*aCDL==aCD1) { temps = aCDL->GetContent(); break; }
            if (*aCDL==aCD2) { temps = ""; break; }
            aCDL=(TCDLib*)aCDL->GetNext();
        }
    }
    return temps;
}


typestr TEvaluateRule::PreviousSubField( TypeCD* CD1, TypeCD* CD2 ) // Idem
{
    FinishCD(CD1);
    FinishCD(CD2);

    TCDLib* aCDL = InputCDL;
    TCD     aCD1(CD1, itsErrorHandler);
    typestr temps = "";

    if (aCDL) aCDL=(TCDLib*)aCDL->GetPrevious();
    if (CD2==NULL)
    {
        if (InputRecord->PreviousCD(&aCDL,&aCD1))
            temps = aCDL->GetContent();
    }
    else
    {
        TCD   aCD2(CD2, itsErrorHandler);
        while(aCDL)
        {
            if (*aCDL==aCD1) { temps = aCDL->GetContent(); break; }
            if (*aCDL==aCD2) { temps = ""; break; }
            aCDL=(TCDLib*)aCDL->GetPrevious();
        }
    }
    return temps;
}

bool TEvaluateRule::CompareOccurrence(TypeInst* aCondition, int aOccurrence)
{
    ToString(aCondition);

    char *p = aCondition->str.str();

    typestr occOper;
    typestr occValue;
    bool found_num = false;
    while (*p)
    {
        if (*p != ' ')
        {
            if (found_num || isalnum(*p))
            {
                found_num = true;
                occValue.append_char(*p);
            }
            else
            {
                occOper.append_char(*p);
            }
        }
        ++p;
    }
    if (!occValue.str())
        return false;
    if (!occOper.str())
        occOper = "=";

    int occValueNum = atoi(occValue.str());

    return ((aOccurrence == occValueNum && strcmp(occOper.str(), "=") == 0) ||
        (aOccurrence != occValueNum && strcmp(occOper.str(), "!=") == 0) ||
        (aOccurrence > occValueNum && strcmp(occOper.str(), ">") == 0) ||
        (aOccurrence >= occValueNum && strcmp(occOper.str(), ">=") == 0) ||
        (aOccurrence < occValueNum && strcmp(occOper.str(), "<") == 0) ||
        (aOccurrence <= occValueNum && strcmp(occOper.str(), "<=") == 0));
}

typestr TEvaluateRule::Table( char* Nom, char* str )
{
    TRuleFile *aRuleFile=RuleDoc->GetFile();
    TCodedData *aCodedData=aRuleFile->GetCodedData(Nom);
    if (!aCodedData)
    {
        // ERROR
        typestr s;
        s.str(str);
        return s;
    }

    typestr s;
    aCodedData->Transcode(str, s, (InputRecord) ? InputRecord->GetFirstField()->GetLib() : NULL, "???");
    return s;
}


void TEvaluateRule::ResetSort()
{
    if (ListSort) delete ListSort;
    ListSort=NULL;
}

int TEvaluateRule::MustSort( char* n )
{
    if (ListSort)
    {
        LastSort->SetNext(new SortElem(mCDOut, n));
        LastSort=LastSort->GetNext();
    }
    else
        ListSort=LastSort=new SortElem(mCDOut, n);
    return 0;
}


int TEvaluateRule::SortRecord(TUMRecord* aRecord)
{
    SortElem* aElem=ListSort;
    while( aElem )
    {
        aRecord->SortCD( aElem->GetCD(), aElem->GetSubFieldList() );
        aElem=aElem->GetNext();
    }
    ResetSort();
    return 0;
}

//
// Renvoie la balise du CDLib suivant le CDLib courant, dans le champ courant
//
const char* TEvaluateRule::NextBalise()
{
    TCDLib* aCDL=InputCDL;
    TCDLib* Suivant;

    Suivant=(TCDLib*)aCDL->GetNext();
    if (Suivant!=NULL)
    {
        if (strcmp(aCDL->GetTag(),Suivant->GetTag())==0)
        {
            return &Suivant->GetSubfield()[1];
        }
    }
    return NULL;
}

//
// Renvoie la balise du CDLib pr�c�dant le CDLib courant, dans le champ courant
//
const char* TEvaluateRule::PreviousBalise()
{
    TCDLib* aCDL=InputCDL;
    TCDLib* Precedant;

    Precedant=(TCDLib*)aCDL->GetPrevious();
    if (Precedant!=NULL)
    {
        if (strcmp(aCDL->GetTag(),Precedant->GetTag())==0)
        {
            return &Precedant->GetSubfield()[1];
        }
    }
    return NULL;
}


TypeCD* TEvaluateRule::AllocCD()
{
    return m_allocator.AllocTypeCD();
}

void TEvaluateRule::FreeCD( TypeCD* CD )
{
    m_allocator.FreeTypeCD(CD);
};

void TEvaluateRule::PrintDebug(const char *s)
{
    if (debug_rule)
        printf("Debug:   %s\n", s);
};


/*
Affichage d'une valeur d' Instruction
*/
char* TEvaluateRule::PrintT( TypeInst* t, char* buf )
{
    if (t->str.str()) sprintf(buf,"<%s>",t->str.str());
    else        sprintf(buf,"(%d)",t->val);
    return buf;
};

/*
NEXT ( subfield [,subfield] [,STRICT] )
*/
TypeInst* TEvaluateRule::Next_( TypeCD* cd1, TypeCD* cd2, int strict )
{
    typestr temps = NextSubField(cd1, cd2);
    if (!temps.str()) 
    {
        FreeCD(cd1);
        FreeCD(cd2);
        return NULL;
    }

    TypeInst* rc = AllocTypeInst();
    if (!strict)
        trim_string(rc->str, temps);
    else
        rc->str = temps;

    FreeCD(cd1);
    FreeCD(cd2);
    return rc;
};

/*
LAST ( subfield [,subfield] [,STRICT] )
*/
TypeInst* TEvaluateRule::Last_( TypeCD* cd1, TypeCD* cd2, int strict )
{
    typestr temps = PreviousSubField( cd1, cd2 );
    if (!temps.str())
    {
        FreeCD(cd1);
        FreeCD(cd2);
        return NULL;
    }

    TypeInst* rc = AllocTypeInst();
    if (!strict)
        trim_string(rc->str, temps);
    else
        rc->str = temps;

    FreeCD(cd1);
    FreeCD(cd2);
    return rc;
};

TypeInst* TEvaluateRule::NextSub(TypeCD* aFindCD, TypeInst *aOccurrence)
{
    TypeInst* rc;
    const char *ptr = NULL;

    if (!aFindCD)
    {
        ptr = NextBalise();
    }
    else
    {
        FinishCD(aFindCD);

        TUMRecord* record = aFindCD->Output ? RealOutputRecord : InputRecord;
        TCDLib *CDL = record->GetFirstCDLib();
        TCD     findCD(aFindCD, itsErrorHandler);

        while (record->NextCD(&CDL, &findCD))
        {
            if (!aOccurrence || CompareOccurrence(aOccurrence, CDL->GetSubOccurrenceNumber()))
            {
                TCD *nextCD = CDL->GetNext();
                if (nextCD && *nextCD->GetSubfield() == '$')
                    ptr = &nextCD->GetSubfield()[1];
                break;
            }
            CDL = (TCDLib *)CDL->GetNext();
        }
    }

    rc=AllocTypeInst();
    rc->str.str(ptr ? ptr : "");
    return rc;
}

TypeInst* TEvaluateRule::NextSubIn(TypeInst* aStr, TypeCD* aFindCD, TypeInst* aOccurrence)
{
    FinishTCD(aFindCD);
    char subfield[3];
    subfield[0] = START_OF_FIELD;
    subfield[1] = aFindCD->SubField[1];
    subfield[2] = '\0';

    TypeInst* rc = AllocTypeInst();
    
    char* p = strstr(aStr->str.str(), subfield);
    int occurrence = 0;
    while (p)
    {
        ++occurrence;
        if (!aOccurrence || CompareOccurrence(aOccurrence, occurrence))
        {
            char* p2 = strchr(p + 1, START_OF_FIELD);
            if (p2)
                rc->str.append_char(*(p2 + 1));
            else
                rc->str = "";
            break;
        }
    }
    return rc;
}

TypeInst* TEvaluateRule::PreviousSub(TypeCD* aFindCD, TypeInst *aOccurrence)
{
    TypeInst* rc;
    const char *ptr = NULL;

    if (!aFindCD)
    {
        ptr = PreviousBalise();
    }
    else
    {
        FinishCD(aFindCD);

        TUMRecord* record = aFindCD->Output ? RealOutputRecord : InputRecord;
        TCDLib *CDL = record->GetFirstCDLib();
        TCD     findCD(aFindCD, itsErrorHandler);

        while (record->NextCD(&CDL, &findCD))
        {
            if (!aOccurrence || CompareOccurrence(aOccurrence, CDL->GetSubOccurrenceNumber()))
            {
                TCD *prevCD = CDL->GetPrevious();
                if (prevCD && *prevCD->GetSubfield() == '$')
                    ptr = &prevCD->GetSubfield()[1];
                break;
            }
            CDL = (TCDLib *)CDL->GetNext();
        }
    }

    rc=AllocTypeInst();
    rc->str.str(ptr ? ptr : "");
    return rc;
};

TypeInst* TEvaluateRule::PreviousSubIn(TypeInst* aStr, TypeCD* aFindCD, TypeInst* aOccurrence)
{
    FinishTCD(aFindCD);

    TypeInst* rc = AllocTypeInst();

    char prev_subfield = '\0';
    char* p = strchr(aStr->str.str(), START_OF_FIELD);
    int occurrence = 0;
    while (p)
    {
        if (*(p + 1) == aFindCD->SubField[1])
        {
            ++occurrence;
            if (!aOccurrence || CompareOccurrence(aOccurrence, occurrence))
            {
                if (prev_subfield)
                    rc->str.append_char(prev_subfield);
                else
                    rc->str = "";
                break;
            }
        }
        prev_subfield = *(p + 1);
        p = strchr(p + 1, START_OF_FIELD);
    }
    return rc;
}

/*
Instruction - Instruction
*/
TypeInst* TEvaluateRule::Subtract( TypeInst* t1, TypeInst* t2 )
{
    if (t1->str.str() || t2->str.str())
    {
        yyerror("Subtraction can not be done between strings");
        FreeTypeInst(t1);
        FreeTypeInst(t2);
        return NULL;
    }
    else
    {
        TypeInst* rc=AllocTypeInst();
        rc->val=t1->val-t2->val;
        FreeTypeInst(t1);
        FreeTypeInst(t2);
        return rc;
    }
}

/*
Instruction * Instruction
*/
TypeInst* TEvaluateRule::Multiply( TypeInst* t1, TypeInst* t2 )
{
    if (t1->str.str() || t2->str.str())
    {
        yyerror("Multiplication can not be done between strings");
        return NULL;
    }
    else
    {
        TypeInst* rc = AllocTypeInst();
        rc->val = t1->val * t2->val;
        FreeTypeInst(t1);
        FreeTypeInst(t2);
        return rc;
    }
}

/*
Instruction : Instruction
*/
TypeInst* TEvaluateRule::Divide( TypeInst* t1, TypeInst* t2 )
{
    if (t1->str.str() || t2->str.str())
    {
        yyerror("Division can not be done between strings");
        FreeTypeInst(t1);
        FreeTypeInst(t2);
        return NULL;
    }
    else
        if (t2->val == 0)
        {
            yyerror("Division by 0 error");
            FreeTypeInst(t1);
            FreeTypeInst(t2);
            return NULL;
        }
        else
        {
            TypeInst* rc = AllocTypeInst();
            rc->val = t1->val / t2->val;
            FreeTypeInst(t1);
            FreeTypeInst(t2);
            return rc;
        }
}

/*
VAL( Instruction )
*/
TypeInst* TEvaluateRule::Value(TypeInst* t)
{
    if (!t->str.str()) 
        return t;

    char* p = t->str.str();
    while (p)
    {
        if (!isdigit(*p))
        {
            typestr msg = t->str;
            msg += " cannot be converted to integer";
            yyerror(msg.str());
            return NULL;
        }
    }
    t->val = atoi(t->str.str());
    t->str.freestr();
    return t;
}

/*
STO(i)
*/
int TEvaluateRule::MemSto( TypeInst* a_index, TypeInst* a_content )
{
    if (a_index->str.str())
    {
        yyerror("Sto(<string>) Index must be numeric");
        FreeTypeInst(a_index);
        FreeTypeInst(a_content);
        return 0;
    }
    int i = a_index->val;
    FreeTypeInst(a_index);
    if (i < 0 || i > 99)
    {
        char tmp[100];
        sprintf(tmp,"Sto(%d) Index out of bounds", i);
        yyerror(tmp);
        FreeTypeInst(a_content);
        return 0;
    }
    if (Memoire[i]) 
        FreeTypeInst(Memoire[i]);
    Memoire[i] = NULL;
    CopyInst(&Memoire[i], a_content ? a_content : S);
    FreeTypeInst(a_content);
    return 0;
}

/*
MEM(i)
*/
TypeInst* TEvaluateRule::MemMem( TypeInst* n )
{
    int i=n->val;
    if (n->str.str())
    {
        yyerror("Mem(<string>) Index must be numeric");
        return NULL;
    }
    else
        if (n->val<0 || n->val>99)
        {
            char tmp[100];
            sprintf(tmp,"Mem(%d) Index out of bounds",n->val);
            yyerror(tmp);
            return NULL;
        }
        if (Memoire[i]!=NULL)
        {
            TypeInst* rc;
            CopyInst(&rc, Memoire[i]);
            FreeTypeInst(n);
            return rc;
        }
        else
        {
            char tmp[100];
            sprintf(tmp,"Mem(%d) is empty",i);
            yyerror(tmp);
            return NULL;
        }
}

/*
CLR(i)
*/
int TEvaluateRule::MemClr( TypeInst* n  )
{
    int i=n->val;
    if (n->str.str())
    {
        yyerror("Clr( String ) is not correct");
        return 0;
    }
    else
        if (n->val<0 || n->val>99)
        {
            char tmp[100];
            sprintf(tmp,"Clr(%d) is not correct",n->val);
            yyerror(tmp);
            return 0;
        }
        FreeTypeInst(Memoire[i]);
        Memoire[i]=NULL;
        FreeTypeInst(n);
        return 0;
}

/*
EXC(i)
*/
TypeInst* TEvaluateRule::MemExc( TypeInst* n )
{
    int i=n->val;
    if (n->str.str())
    {
        yyerror("Exc(<string>) Index must be numeric");
        return NULL;
    }
    else
        if (n->val<0 || n->val>99)
        {
            char tmp[100];
            sprintf(tmp,"Exc(%d) Index out of bounds",n->val);
            yyerror(tmp);
            return NULL;
        }
        if (Memoire[i]!=NULL)
        {
            TypeInst* rc=Memoire[i];
            CopyInst(&Memoire[i], S);
            FreeTypeInst(n);
            return rc;
        }
        else
        {
            char tmp[100];
            sprintf(tmp,"Exc(%d) is empty",i);
            yyerror(tmp);
            return NULL;
        }
}

TypeInst* TEvaluateRule::AllocTypeInst()
{
    return m_allocator.AllocTypeInst();
}

void TEvaluateRule::FreeTypeInst( TypeInst* t )
{
    m_allocator.FreeTypeInst(t);
}

int TEvaluateRule::CopyInst( TypeInst** In, TypeInst* From )
{
    *In=AllocTypeInst();
    if (From==NULL || From->str.str()==NULL)
    {
        (*In)->str.freestr();
    }
    else
    {
        (*In)->str.str(From->str.str());
    }
    if (From)
    {
        (*In)->val=From->val;
    }
    return 0;
}


/*
Comparaison de deux instructions
*/
int TEvaluateRule::BoolEQ( TypeInst* t1, TypeInst* t2 )
{
    int rc;
    char b1[200], b2[200];

    if (t1->str.str()==NULL && t2->str.str()==NULL) rc=t1->val==t2->val;
    else rc=!strcmp(ToString(t1),ToString(t2));
    if (rc) rc=TRUE;
    else    rc=FALSE;
    if (debug_rule) 
        printf("Debug:    %s = %s => %d\n",PrintT(t1,b1), PrintT(t2,b2),rc);
    FreeTypeInst(t1);
    FreeTypeInst(t2);
    return rc;
}


/*
Comparaison de contenance de deux instructions
*/
int TEvaluateRule::BoolIn( TypeInst* t1, TypeInst* t2 )
{
    int rc;
    char b1[200], b2[200];

    if (strstr(ToString(t2),ToString(t1))) rc=TRUE;
    else                       rc=FALSE;
    if (debug_rule) 
        printf("Debug:    %s IN %s => %d\n",PrintT(t1,b1), PrintT(t2,b2),rc);
    FreeTypeInst(t1);
    FreeTypeInst(t2);
    return rc;
}


/*
Comparaison de deux instructions
*/
int TEvaluateRule::BoolGT( TypeInst* t1, TypeInst* t2 )
{
    int rc;
    char b1[200], b2[200];

    if (t1->str.str()==NULL && t2->str.str()==NULL) rc=t1->val-t2->val;
    else rc=strcmp(ToString(t1),ToString(t2));
    if (rc>0) rc=TRUE;
    else    rc=FALSE;
    if (debug_rule) 
        printf("Debug:    %s > %s => %d\n",PrintT(t1,b1), PrintT(t2,b2),rc);
    FreeTypeInst(t1);
    FreeTypeInst(t2);
    return rc;
}


/*
Comparaison de deux instructions
*/
int TEvaluateRule::BoolGE( TypeInst* t1, TypeInst* t2 )
{
    int rc;
    char b1[200], b2[200];

    if (t1->str.str()==NULL && t2->str.str()==NULL) rc=t1->val-t2->val;
    else rc=strcmp(ToString(t1),ToString(t2));
    if (rc>=0) rc=TRUE;
    else    rc=FALSE;
    if (debug_rule) 
        printf("Debug:    %s >= %s => %d\n",PrintT(t1,b1), PrintT(t2,b2),rc);
    FreeTypeInst(t1);
    FreeTypeInst(t2);
    return rc;
}


/*
Instruction + Instruction
*/
TypeInst* TEvaluateRule::Add( TypeInst* t1, TypeInst* t2 )
{
    TypeInst* rc = AllocTypeInst();
    if (!t1->str.str() && !t2->str.str())
    {
        rc->val = t1->val + t2->val;
    }
    else
    {
        rc->val = 0;
        rc->str = t1->str;
        rc->str += t2->str.str() ? t2->str : ToString(t2);
    }
    FreeTypeInst(t1);
    FreeTypeInst(t2);

    return rc;
}

/*
200(1)
*/
TypeInst* TEvaluateRule::AddOcc( TypeInst* t1, TypeInst* t2 )
{
    TypeInst* rc = AllocTypeInst();
    rc->val = 0;
    rc->str = t1->str;
    char tmp[30];
    sprintf(tmp, "(%d)", t2->val);
    rc->str += tmp;
    FreeTypeInst(t1);
    FreeTypeInst(t2);

    return rc;
}

/*
Conversion de numerique en char* si necessaire pour une Instruction
*/
char* TEvaluateRule::ToString(TypeInst* t)
{
    if (!t) 
        return NULL;

    if (!t->str.str())
    {
        char tmp[80];
        sprintf(tmp, "%d", t->val);
        t->str.str(tmp);
    }
    return t->str.str();
}

/*
STR( Instruction )
*/
TypeInst* TEvaluateRule::String( TypeInst* t )
{
    ToString(t);
    return t;
};

/*
UPPER( Instruction )
*/
TypeInst* TEvaluateRule::Upper( TypeInst* t )
{
    ToString(t);
    char *p = t->str.str();
    while (*p)
    {
        *p = toupper(*p);
        ++p;
    }
    return t;
};

/*
LOWER( Instruction )
*/
TypeInst* TEvaluateRule::Lower( TypeInst* t )
{
    ToString(t);
    char *p = t->str.str();
    while (*p)
    {
        *p = tolower(*p);
        ++p;
    }
    return t;
};

/*
LEN( Instruction )
*/
TypeInst* TEvaluateRule::Len( TypeInst* t )
{
    TypeInst* rc=AllocTypeInst();
    if (t->str.str()==NULL)
        rc->val=0;
    else
    {
        if (itsUTF8Mode)
        {
            rc->val = utf8_strlen(t->str.str());
        }
        else
        {
            rc->val=strlen(t->str.str());
        }
    }
    FreeTypeInst(t);
    return rc;
};

/*
FROM( translation [, STRICT] )
*/
TypeInst* TEvaluateRule::From( TypeInst* t, int strict )
{
    TypeInst*rc;
    unsigned int i;

    Value(t);
    i=(unsigned int)(t->val-1);
    rc=AllocTypeInst();
    ToString(S);
    unsigned int slen = itsUTF8Mode ? utf8_strlen(S->str.str()) : strlen(S->str.str());
    if (t->str.str() || i<0 || i>slen)
    {
        rc->str.str("");
    }
    else
    {
        if (itsUTF8Mode)
        {
            i = utf8_charindex(S->str.str(), i);
        }
        char *p = S->str.str() + i;
        if (!strict)
        {
            while(*p == ' ') 
                ++p;
        }
        rc->str.str(p);
    }
    rc->val=0;
    FreeTypeInst(t);
    return rc;
};

/*
TO( translation [, STRICT] )
*/
TypeInst* TEvaluateRule::To( TypeInst* t, int strict )
{
    TypeInst*rc;
    unsigned int i;

    Value(t);
    i=(unsigned int)(t->val-1);
    rc=AllocTypeInst();
    ToString(S);
    unsigned int slen = itsUTF8Mode ? utf8_strlen(S->str.str()) : strlen(S->str.str());
    if (t->str.str() || i<0 || i>slen)
    {
        rc->str.str("");
    }
    else
    {
        rc->str.str(S->str.str());
        char *p = rc->str.str();
        ++i;
        if (itsUTF8Mode)
        {
            i = utf8_charindex(p, i);
            p += i;
        }
        else
        {
            p += i;
        }
        *p = '\0';
        if (!strict)
        {
            p = rc->str.str() + strlen(rc->str.str());
            while(*p == ' ' && p > rc->str.str())
            {
                *p = '\0';
                --p;
            }
        }
    }
    rc->val = 0;
    FreeTypeInst(t);
    return rc;
};

/*
BETWEEN( translation , translation [, STRICT] )
*/
TypeInst* TEvaluateRule::Between( TypeInst* t1, TypeInst* t2, int strict )
{
    TypeInst *rc;
    unsigned int i1,i2;

    /* Suite a la remarque no 80 */
    Value(t1);
    Value(t2);
    i1=(unsigned int)(t1->val-1);
    i2=(unsigned int)(t2->val-1);
    rc=AllocTypeInst();
    ToString(S);
    unsigned int slen = itsUTF8Mode ? utf8_strlen(S->str.str()) : strlen(S->str.str());
    if (t1->str.str() || i1<0 || i1>slen ||
        t2->str.str() || i2<0 || i2>slen ||
        i1 > i2) // DanX, Talis: Check added 20050610
    {
        rc->str.str("");
    }
    else
    {
        if (itsUTF8Mode)
        {
            i1 = utf8_charindex(S->str.str(), i1);
            i2 = utf8_charindex(S->str.str(), i2);
        }
        if (!strict)
        {
            while((S->str.str()[i1]) == ' ' ) ++i1;
            while((S->str.str()[i2]) == ' ' ) --i2;
        }
        ++i2;
        if( i1 <= i2 )
        {
            rc->str.allocstr(i2-i1+1);
            memcpy(rc->str.str(),&S->str.str()[i1],i2-i1);
            rc->str.str()[i2-i1]=0;
        }
    }
    rc->val=0;
    FreeTypeInst(t1);
    FreeTypeInst(t2);
    return rc;
};

/*
REPLACE( translation BY translation [, AT ...] [, STRICT] )
*/
TypeInst* TEvaluateRule::Replace( TypeInst* t1, TypeInst* t2, IN_STR_POSITION at, bool strict )
{
    ToString(S);
    typestr str = S->str;
    typestr source = ToString(t1);
    typestr replacement = t2 ? ToString(t2) : "";
    FreeTypeInst(t1);
    FreeTypeInst(t2);
    int source_len = strlen(source.str());
    int replacement_len = strlen(replacement.str());
    
    if (source_len > 0) 
    {
        switch (at) 
        {
        case SP_ANY:
            {
                char* p = strstr(str.str(), source.str());
                while (p)
                {
                    typestr tmp;
                    int pos = p - str.str();
                    if (pos > 0)
                        tmp.str(str.str(), pos);
                    tmp += replacement;
                    p += source_len;
                    tmp += p;
                    str = tmp;
                    p = strstr(str.str() + pos + replacement_len, source.str());
                }
                break;
            }
        case SP_BEGINNING:
        case SP_BOTH:
            {
                char* p = str.str();
                while (strncmp(p, source.str(), source_len) == 0)
                {
                    typestr tmp;
                    int pos = p - str.str();
                    if (pos > 0)
                        tmp.str(str.str(), pos);
                    tmp += replacement;
                    p += strlen(source.str());
                    tmp += p;
                    str = tmp;
                    p = str.str() + pos + replacement_len;
                }
                if (at == SP_BEGINNING)
                    break;
            }
        case SP_END:
            {
                int len = strlen(str.str());
                if (len < source_len)
                    break;
                char* p = str.str() + len - source_len;
                while (strncmp(p, source.str(), source_len) == 0)
                {
                    typestr tmp;
                    int pos = p - str.str();
                    if (pos > 0)
                        tmp.str(str.str(), pos);
                    tmp += replacement;
                    p += strlen(source.str());
                    tmp += p;
                    str = tmp;
                    if (pos - replacement_len < 0)
                        break;
                    p = str.str() + pos - replacement_len;
                }
                break;
            }
        }
    }

    TypeInst* rc = AllocTypeInst();
    rc->val = 0;
    if (!strict)
        trim_string(rc->str, str);
    else
        rc->str = str;
    return rc;
}

/*
REPLACEOCC( translation BY translation , OCCURRENCE  [, STRICT] )
*/
TypeInst* TEvaluateRule::ReplaceOcc( TypeInst* t1, TypeInst* t2, TypeInst* inCondOcc, bool strict )
{
    typestr str = ToString(S);
    typestr source = ToString(t1);
    typestr replacement = t2 ? ToString(t2) : "";
    int source_len = strlen(source.str());
    int replacement_len = strlen(replacement.str());
    FreeTypeInst(t1);
    FreeTypeInst(t2);

    int occurrence = 0;
    char* p = strstr(str.str(), source.str());
    while (p)
    {
        if (CompareOccurrence(inCondOcc, ++occurrence))
        {
            typestr tmp;
            int pos = p - str.str();
            if (pos > 0)
                tmp.str(str.str(), pos);
            tmp += replacement;
            tmp += p + source_len;
            str = tmp;
            p = strstr(str.str() + pos + replacement_len, source.str());
        }
        else
            p = strstr(p + 1, source.str());

    }
    FreeTypeInst(inCondOcc);

    TypeInst* rc = AllocTypeInst();
    rc->val = 0;
    if (!strict)
        trim_string(rc->str, str);
    else
        rc->str = str;
    return rc;
};

int TEvaluateRule::IsDigit( char c )
{
    unsigned char uc=(unsigned char)c;
    return isdigit((int)uc);
};

/*
BFIRST( translation | ... )
*/
TypeInst* TEvaluateRule::BFirst( TypeInst* t, int k )
{
    TypeInst*rc;
    int i;
    int l;

    rc=AllocTypeInst();
    ToString(S);

    switch(k)
    {
    case 0 :
        ToString(t);
        l=strlen(t->str.str());
        i=0;
        while( S->str.str()[i] )
        {
            if (memcmp(&S->str.str()[i],t->str.str(),l)) ++i;
            else break;
        }
        break;

    case 1 :
        i=0;
        while( S->str.str()[i] )
        {
            if (IsDigit(S->str.str()[i])) break;
            else ++i;
        }
        break;

    case 2 :
        i=0;
        while( S->str.str()[i] )
        {
            if ((unsigned char)(S->str.str()[i])==' ' || IsDigit(S->str.str()[i])) ++i;
            else break;
        }
        break;
    }
    if (S->str.str()[i])
        rc->val=i+1;
    else
        rc->val=0;
    FreeTypeInst(t);
    return rc;
};

/*
EFIRST( translation | ... )
*/
TypeInst* TEvaluateRule::EFirst( TypeInst* t, int k )
{
    TypeInst*rc;
    int i,ok;
    int l;

    rc=AllocTypeInst();
    ToString(S);

    ok=0;
    switch(k)
    {
    case 0 :
        ToString(t);
        l=strlen(t->str.str());
        i=0;
        while( S->str.str()[i] )
        {
            if (memcmp(&S->str.str()[i],t->str.str(),l)) ++i;
            else
            {
                ok=1;
                break;
            }
        }
        if (S->str.str()[i]) i+=l;
        break;

    case 1 :
        i=0;
        while( S->str.str()[i] )
        {
            if (IsDigit(S->str.str()[i]))
            {
                ok=1;
                break;
            }
            else ++i;
        }
        while( S->str.str()[i] )
        {
            if (IsDigit(S->str.str()[i])) ++i;
            else break;
        }
        break;

    case 2 :
        i=0;
        while( S->str.str()[i] )
        {
            if ((unsigned char)(S->str.str()[i])==' ' || IsDigit(S->str.str()[i])) ++i;
            else
            {
                ok=1;
                break;
            }
        }
        while( S->str.str()[i] )
        {
            if ((unsigned char)(S->str.str()[i])!=' ' && !IsDigit(S->str.str()[i])) ++i;
            else break;
        }
        break;
    }
    if (ok)
        rc->val=i+1;
    else
        rc->val=0;
    FreeTypeInst(t);
    return rc;
};

/*
BLAST( translation | ... )
*/
TypeInst* TEvaluateRule::BLast( TypeInst* t, int k )
{
    TypeInst*rc;
    int i,ok;
    int l;

    rc=AllocTypeInst();
    ToString(S);
    ok=0;

    switch(k)
    {
    case 0 :
        ToString(t);
        l=strlen(t->str.str());
        i=strlen(S->str.str()) - 1;
        while( i>=0 )
        {
            if (memcmp(&S->str.str()[i],t->str.str(),l)) --i;
            else
            {
                ok=1;
                break;
            }
        }
        break;

    case 1 :
        i=strlen(S->str.str());
        while( i>=0 )
        {
            if (IsDigit(S->str.str()[i]))
            {
                ok=1;
                break;
            }
            else --i;
        }
        while( i>0 )
        {
            if (IsDigit(S->str.str()[i])) --i;
            else break;
        }
        break;

    case 2 :
        i=strlen(S->str.str());
        while( i>=0 )
        {
            if ((unsigned char)(S->str.str()[i])==' ' || IsDigit(S->str.str()[i])) --i;
            else
            {
                ok=1;
                break;
            }
        }
        while( i>0 )
        {
            if ((unsigned char)(S->str.str()[i])!=' ' && !IsDigit(S->str.str()[i])) --i;
            else break;
        }
        break;
    }
    if (ok)
        rc->val=i+1;
    else
        rc->val=0;
    FreeTypeInst(t);
    return rc;
};

/*
ELAST( translation | ... )
*/
TypeInst* TEvaluateRule::ELast( TypeInst* t, int k )
{
    TypeInst*rc;
    int i;
    int l;

    rc=AllocTypeInst();
    ToString(S);

    switch(k)
    {
    case 0 :
        ToString(t);
        l=strlen(t->str.str());
        i=strlen(S->str.str())-l;
        while( i>=0 )
        {
            if (memcmp(&S->str.str()[i],t->str.str(),l)) --i;
            else break;
        }
        if (i>=0) i+=l;
        break;

    case 1 :
        i=strlen(S->str.str());
        while( i>=0 )
        {
            if (IsDigit(S->str.str()[i])) break;
            else --i;
        }
        break;

    case 2 :
        i=strlen(S->str.str());
        while( i>=0 )
        {
            if ((unsigned char)(S->str.str()[i])==' ' || IsDigit(S->str.str()[i])) --i;
            else break;
        }
        break;
    }
    if (i>=0)
        rc->val=i+1;
    else
        rc->val=0;
    FreeTypeInst(t);
    return rc;
};

/*
TABLE ( subfield [,subfield] [,STRICT] )
*/
TypeInst* TEvaluateRule::Table_( TypeInst* Nom )
{
    TypeInst* rc;

    rc=AllocTypeInst();
    rc->str = Table( Nom->str.str(), S->str.str() );
    FreeTypeInst( Nom );
    return rc;
};

// TODO: make this use the RegExp class
bool TEvaluateRule::RegFindInternal(const char *a_str, const char *a_regexp)
{
    int re_opts = itsUTF8Mode ? PCRE_UTF8 : 0;
    int error_code = 0;
    const char *error_ptr = NULL;
    int error_offset = 0;
    pcre* re = pcre_compile2(a_regexp, re_opts, &error_code, &error_ptr, &error_offset, NULL);
    if (!re)
    {
        typestr error = "Could not compile regular expression '";
        error += a_regexp;
        error += "': ";
        error += itsErrorHandler->GetPCREErrorDesc(error_code);
        if (error_ptr)
        {
            error += " at ";
            error += error_ptr;
        }
        error += " (offset ";
        char offset_str[30];
        sprintf(offset_str, "%d", error_offset);
        error += offset_str;
        error += ")";
        yyerror(error.str());
        return false;
    }

    mRegExpSearchString = a_str;

    mRegExpResult = pcre_exec(re, NULL, mRegExpSearchString.str(), strlen(mRegExpSearchString.str()), 0, 0, mRegExpMatchVector, sizeof(mRegExpMatchVector) / sizeof(int));
    pcre_free(re);
    if (mRegExpResult == 0)
        mRegExpResult = sizeof(mRegExpMatchVector) / sizeof(int) / 3;
    else if (mRegExpResult < 0 && mRegExpResult != PCRE_ERROR_NOMATCH)
    {
        char ret_str[30];
        sprintf(ret_str, "%d", mRegExpResult);
        typestr error = "Evaluation of regular expression '";
        error += a_regexp;
        error += "', error code ";
        error += ret_str;
        yyerror(error.str());
        return false;
    }
    return true;
}

TypeInst* TEvaluateRule::RegFindNum( TypeInst* t1, TypeInst* t2 )
{
    TypeInst *rc = AllocTypeInst();
    rc->val = -1;
    ToString(S);
    ToString(t1);

    typestr search_string;
    typestr regexp;
    if (t2)
    {
        ToString(t2);
        search_string = t1->str;
        regexp = t2->str;
        FreeTypeInst(t2);
    }
    else
    {
        search_string = S->str;
        regexp = t1->str;
    }
    FreeTypeInst(t1);

    if (RegFindInternal(search_string.str(), regexp.str()))
    {
        rc->val = (mRegExpResult == PCRE_ERROR_NOMATCH) ? 0 : mRegExpResult;
    }

    return rc;
}


TypeInst* TEvaluateRule::RegFindPos( TypeInst* t1, TypeInst* t2 )
{
    TypeInst *rc = AllocTypeInst();
    rc->val = -1;
    ToString(S);
    ToString(t1);

    typestr search_string;
    typestr regexp;
    if (t2)
    {
        ToString(t2);
        search_string = t1->str;
        regexp = t2->str;
        FreeTypeInst(t2);
    }
    else
    {
        search_string = S->str;
        regexp = t1->str;
    }
    FreeTypeInst(t1);

    if (RegFindInternal(search_string.str(), regexp.str()))
    {
        if (mRegExpResult != PCRE_ERROR_NOMATCH)
            rc->val = mRegExpMatchVector[0] + 1;
    }

    return rc;
}

TypeInst* TEvaluateRule::RegFind( TypeInst* t1, TypeInst* t2 )
{
    TypeInst *rc = RegFindNum(t1, t2);
    if (rc->val >= 0)
        --rc->val;
    return rc;
}

/*
RegMatch( translation )
*/
TypeInst* TEvaluateRule::RegMatch( TypeInst* t1 )
{
    if (t1->str.str())
    {
        yyerror("RegMatch() requires a numeric index");
        FreeTypeInst(t1);
        return NULL;
    }
    int index = t1->val;
    FreeTypeInst(t1);
    TypeInst *rc = AllocTypeInst();
    if (index < 0 || index >= mRegExpResult)
        rc->str.str("");
    else
    {
        rc->str.str(mRegExpSearchString.str() + mRegExpMatchVector[2 * index], mRegExpMatchVector[2 * index + 1]);
    }
    return rc;
}

bool TEvaluateRule::RegReplaceInternal(typestr &a_str, const char *a_regexp, const char *a_replacement, bool a_global)
{
    int loop_count = 0;
    int start_pos = 0;
    while (loop_count++ < 10000)
    {
        if (!RegFindInternal(a_str.str() + start_pos, a_regexp))
        {
            return false;
        }
        if (mRegExpResult == PCRE_ERROR_NOMATCH)
            break;
        
        typestr replacement;
        const char *p = a_replacement;
        while (*p)
        {
            if (*p == '\\')
            {
                ++p;
                if (*p == '\\')
                    replacement.append_char('\\');
                else if (isdigit(*p))
                {
                    int index = *p - '0';
                    replacement.append(mRegExpSearchString.str() + mRegExpMatchVector[2 * index], mRegExpMatchVector[2 * index + 1] - mRegExpMatchVector[2 * index]);
                }
            }
            else
                replacement.append_char(*p);
            ++p;
        }
        int replace_start = mRegExpMatchVector[0];
        int replace_end = mRegExpMatchVector[1];
        typestr tmp;
        tmp.str(mRegExpSearchString.str(), replace_start);
        tmp.append(replacement);
        int prev_start_pos = start_pos;
        start_pos = strlen(tmp.str());
        tmp.append(mRegExpSearchString.str() + replace_end);
        *(a_str.str() + prev_start_pos) = '\0';
        a_str.append(tmp);

        if (!a_global)
            break;
    }
    return true;
}

TypeInst* TEvaluateRule::RegReplace(TypeInst* a_regexp, TypeInst* a_replacement, TypeInst* a_options)
{
    ToString(S);
    ToString(a_regexp);
    ToString(a_replacement);
    bool global = false;
    if (a_options)
    {
        ToString(a_options);
        global = strchr(a_options->str.str(), 'g') != NULL;
        FreeTypeInst(a_options);
    }

    typestr dest = S->str;
    if (!RegReplaceInternal(dest, a_regexp->str.str(), a_replacement->str.str(), global))
    {
        typestr error = "Could not compile regular expression '";
        error += a_regexp->str;
        error += "'";
        yyerror(error.str());
        FreeTypeInst(a_regexp);
        FreeTypeInst(a_replacement);
        return NULL;
    }

    TypeInst *rc = AllocTypeInst();
    rc->str = dest;        
    rc->val = 0;
    FreeTypeInst(a_regexp);
    FreeTypeInst(a_replacement);
    return rc;
}

TypeInst* TEvaluateRule::RegReplaceTable(TypeInst* a_table, TypeInst* a_options)
{
    ToString(S);
    ToString(a_table);
    bool global = false;
    if (a_options)
    {
        ToString(a_options);
        global = strchr(a_options->str.str(), 'g') != NULL;
        FreeTypeInst(a_options);
    }

    typestr dest = S->str;
    StringTable *table = RuleDoc->GetFile()->GetStringTable(a_table->str.str());
    if (!table)
    {
        typestr error = "Could not open table '";
        error += a_table->str;
        error += "'";
        yyerror(error.str());
        FreeTypeInst(a_table);
        return NULL;
    }
    FreeTypeInst(a_table);
    while (table)
    {
        if (!RegReplaceInternal(dest, table->m_src.str(), table->m_dst.str(), global))
        {
            typestr error = "Could not compile regular expression '";
            error += table->m_src;
            error += "'";
            yyerror(error.str());
            return NULL;
        }
        table = table->GetNext();
    }

    TypeInst *rc = AllocTypeInst();
    rc->str = dest;        
    rc->val = 0;
    return rc;
}

/*
InTable( translation, translation )
*/
int TEvaluateRule::InTable(TypeInst* t1, TypeInst* table)
{
    ToString(t1);

    int rc = 0;

    TRuleFile *aRuleFile = RuleDoc->GetFile();
    TCodedData *aCodedData = aRuleFile->GetCodedData(table->str.str());
    if (!aCodedData)
    {
        yyerror("Could not open table");
        return rc;
    }

    if (aCodedData->Exists(t1->str.str()))
        rc = 1;

    FreeTypeInst(t1);
    FreeTypeInst(table);
    return rc;
}

bool TEvaluateRule::move_subfields(typestr &a_fielddata, TypeInst* a_source, TypeCD* a_new_pos, bool a_after, TypeInst* a_target, 
                                   TypeInst* a_prefix, TypeInst* a_suffix, TypeInst* a_preserved_punctuations, 
                                   TypeInst* a_preserved_subfields)
{
    if (a_target && !a_target->str.is_empty() && strlen(a_source->str.str()) != strlen(a_target->str.str()))
    {
        yyerror("Source and target list must be of equal length");
        FreeTypeInst(a_source);
        FreeCD(a_new_pos);
        FreeTypeInst(a_target);
        FreeTypeInst(a_prefix);
        FreeTypeInst(a_suffix);
        FreeTypeInst(a_preserved_punctuations);
        return false;
    }

    ToString(a_source);
    ToString(a_target);
    ToString(a_prefix);
    ToString(a_suffix);
    ToString(a_preserved_punctuations);
    ToString(a_preserved_subfields);

    bool result = false;

    typestr fielddata = a_fielddata;
    typestr subfields;

    // Find the data to move and cut it from the field
    bool found_after = false;
    bool found_before = false;
    bool found_subfields = false;
    char* p = strchr(fielddata.str(), START_OF_FIELD);
    while (p)
    {
        bool is_preserved = a_preserved_subfields && !!strchr(a_preserved_subfields->str.str(), *(p + 1));
        if (strchr(a_source->str.str(), *(p + 1)) || (found_subfields && is_preserved))
        {
            found_subfields = true;
            if (!is_preserved && a_target && !a_target->str.is_empty())
            {
                // Convert subfield code
                int source_pos = strchr(a_source->str.str(), *(p + 1)) - a_source->str.str();
                *(p + 1) = *(a_target->str.str() + source_pos);
            }

            char *p_end = strchr(p + 1, START_OF_FIELD);
            bool at_eof = !p_end;
            if (!p_end)
                p_end = p + strlen(p) - 1;

            // Check for any prefix to preserve in the end of the preceding field
            int before_len = p - fielddata.str();
            int subfield_len = p_end - p;
            char* pp = a_preserved_punctuations ? a_preserved_punctuations->str.str() : NULL;
            while (pp)
            {
                char* end_pp = strchr(pp, '|');
                typestr prefix;
                if (end_pp)
                    prefix.str(pp, end_pp - pp);
                else
                    prefix = pp;
                int prefix_len = strlen(prefix.str());

                // Something at the end of the subfield to be moved
                if (!found_after && subfield_len >= prefix_len)
                {
                    char* prep = p_end - prefix_len;
                    if (strncmp(prep, prefix.str(), prefix_len) == 0)
                    {
                        p_end -= prefix_len;
                        found_after = true;
                    }
                }
                // Something before subfield to be moved
                if (!found_before && before_len >= prefix_len)
                {
                    char* prep = p - prefix_len;
                    if (strncmp(prep, prefix.str(), prefix_len) == 0)
                    {
                        p -= prefix_len;
                        found_before = true;
                    }
                }

                if (!end_pp || (found_after && found_before))
                    break;
                pp = end_pp + 1;
            }
    
            if (at_eof)
            {
                subfields += p;
                *p = '\0';
                break;
            }
            else
            {
                subfields.append(p, p_end - p);
                memmove(p, p_end, strlen(p_end) + 1);
                continue;
            }
        }
        else if (found_subfields)
        {
            break;
        }
        p = strchr(p + 1, START_OF_FIELD);
    }

    int occurrence = 0;
    if (!subfields.is_empty())
    {
        // Find the correct position
        char subfield[3];
        subfield[0] = START_OF_FIELD;
        subfield[1] = a_new_pos->SubField[1];
        subfield[2] = '\0';
        p = strstr(fielddata.str(), subfield);
        while (p)
        {
            ++occurrence;
            if (a_new_pos->ns == 0 || a_new_pos->ns == occurrence)
            {
                // Found
                typestr before;
                typestr after;
                if (a_after)
                {
                    p = strchr(p + 1, START_OF_FIELD);
                }

                if (p)
                {
                    int pos = p - fielddata.str();
                    if (pos > 0)
                        before.str(fielddata.str(), pos);
                    after = p;
                }
                else
                {
                    p = fielddata.str();
                    before = fielddata;
                    after = "";
                }

                // Check for any prefix to preserve in the end of the preceding field
                int before_len = strlen(before.str());
                char* pp = a_preserved_punctuations ? a_preserved_punctuations->str.str() : NULL;
                while (pp)
                {
                    char* end_pp = strchr(pp, '|');
                    typestr prefix;
                    if (end_pp)
                        prefix.str(pp, end_pp - pp);
                    else
                        prefix = pp;
                    int prefix_len = strlen(prefix.str());

                    if (before_len >= prefix_len)
                    {
                        char* prep = before.str() + before_len - prefix_len;
                        if (strncmp(prep, prefix.str(), prefix_len) == 0)
                        {
                            *prep = '\0';
                            after = prefix + after;
                            break;
                        }
                    }
                    if (!end_pp)
                        break;
                    pp = end_pp + 1;
                }

                if (a_prefix && !a_prefix->str.is_empty())
                    before += a_prefix->str;
                if (a_suffix && !a_suffix->str.is_empty())
                    subfields += a_prefix->str;

                fielddata = before + subfields + after;
                result = true;
                break;
            }
            p = strstr(p + 1, a_new_pos->SubField);
        }        
    }

    if (result)
        a_fielddata = fielddata;


    FreeTypeInst(a_source);
    FreeCD(a_new_pos);
    FreeTypeInst(a_target);
    FreeTypeInst(a_prefix);
    FreeTypeInst(a_suffix);
    FreeTypeInst(a_preserved_punctuations);

    return result;
}

TypeInst* TEvaluateRule::MoveBefore(TypeInst* a_source, TypeCD* a_before, TypeInst* a_target, 
                                    TypeInst* a_prefix, TypeInst* a_suffix, TypeInst* a_preserved_punctuations, 
                                    TypeInst* a_preserved_subfields)
{
    typestr fielddata = S->str;
    move_subfields(fielddata, a_source, a_before, false, a_target, a_prefix, a_suffix, a_preserved_punctuations, 
        a_preserved_subfields);

    TypeInst *rc = AllocTypeInst();
    rc->str = fielddata;        
    rc->val = 0;
    return rc;
}

TypeInst* TEvaluateRule::MoveAfter(TypeInst* a_source, TypeCD* a_after, TypeInst* a_target, 
                                   TypeInst* a_prefix, TypeInst* a_suffix, TypeInst* a_preserved_punctuations, 
                                   TypeInst* a_preserved_subfields)
{
    typestr fielddata = S->str;
    move_subfields(fielddata, a_source, a_after, true, a_target, a_prefix, a_suffix, a_preserved_punctuations,
        a_preserved_subfields);
        
    TypeInst *rc = AllocTypeInst();
    rc->str = fielddata;        
    rc->val = 0;
    return rc;
}
