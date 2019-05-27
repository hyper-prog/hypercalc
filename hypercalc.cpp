/*  HyperCalc
    High precision command line styled calculator
    http://hyperprog.com/hypercalc/

   (C) 2013 - 2019 Peter Deak  (hyper80@gmail.com)

    License: GPLv2  http://www.gnu.org/licenses/gpl-2.0.html

    hypercalc.cpp
*/
#include "hypercalc.h"
#include <dconsole.h>

#define MAX_DECIMAL 500

#include "boost/multiprecision/number.hpp"
#include "boost/multiprecision/cpp_dec_float.hpp"
#include "boost/multiprecision/cpp_int.hpp"
#include "boost/math/constants/constants.hpp"

typedef boost::multiprecision::cpp_dec_float<MAX_DECIMAL> mp_backend;
typedef boost::multiprecision::number<mp_backend, boost::multiprecision::et_off> Number;

// //////////////////////////////////////////////////////////////////////// //
// Internal functions                                                       //
// //////////////////////////////////////////////////////////////////////// //

Number calculated_pi;

Number hc_round(Number v) { return boost::multiprecision::round(v); }
Number hc_floor(Number v) { return boost::multiprecision::floor(v); }
Number hc_ceil(Number v)  { return boost::multiprecision::ceil(v);  }
Number hc_abs(Number v)   { return boost::multiprecision::abs(v);   }
Number hc_torad(Number v) { return v * calculated_pi/180.0;         }
Number hc_todeg(Number v) { return v * 180.0 / calculated_pi;       }
Number hc_sin(Number v)   { return boost::multiprecision::sin(v * calculated_pi/180.0); }
Number hc_cos(Number v)   { return boost::multiprecision::cos(v * calculated_pi/180.0); }
Number hc_tan(Number v)   { return boost::multiprecision::tan(v * calculated_pi/180.0); }
Number hc_asin(Number v)  { return boost::multiprecision::asin(v) * 180.0 / calculated_pi; }
Number hc_acos(Number v)  { return boost::multiprecision::acos(v) * 180.0 / calculated_pi; }
Number hc_atan(Number v)  { return boost::multiprecision::atan(v) * 180.0 / calculated_pi; }
Number hc_exp(Number v)   { return boost::multiprecision::exp(v);   }
Number hc_log(Number v)   { return boost::multiprecision::log(v);   }
Number hc_log2(Number v)  { return hc_log(v) / hc_log(2);           }
Number hc_log10(Number v) { return boost::multiprecision::log10(v); }
Number hc_sqrt(Number v)
{
    if(v < 0)
        throw QObject::tr("Cannot calculate root of negative number");
    return boost::multiprecision::sqrt(v);
}

Number hc_fact(Number v)
{
    if(v<0)
        throw QObject::tr("Factorial works on positive numbers");
    if(v==0)
        return 1;
    return hc_fact(v-1)*v;
}

// //////////////////////////////////////////////////////////////////////// //
// Definitions                                                              //
// //////////////////////////////////////////////////////////////////////// //

struct UserFunction
{
    QString name;
    QStringList parameters;
    QString body;
    QString libname;
    QString comment;
};

class FuncEnvironment
{
public:
    FuncEnvironment(void);
public:
    QString name;
    QMap<QString,Number> parameters;
    QStringList callStack;
};

//Private Implementation class of HyperCalc
class HyperCalcPrivate
{
    friend class HyperCalc;

private:
    HyperCalcPrivate(HyperCalc *parent);

    HyperCalc *pp;
    HConsolePanel *console;

    QString outputModifier;
    QMap<QString,Number (*)(Number)> functions;
    QMap<QString,QString> describeFuncOutM;
    QList<QString> outmods;
    QList<QString> commands;
    QList<UserFunction> ufunctions;

    int lasts;
    int maxFragmentLetters;
    Number omul;

private:
    void    calcString(QString str);
    void    hintNeeded(QString str);
    void    setPrecision(int digits);
    int     precision(void);

    Number  eval(QString str,int level,int spos,FuncEnvironment& env);
    int     cInS(QString str,QString chars,bool left_to_right);

    void    defineUserFunction(QString str,QString libname="",bool disableNotification=false);
    void    makeFuncEnv(int level,int spos,QString callstring,QString fnName,FuncEnvironment& env,FuncEnvironment& newEnv);
    void    loadFile(QString fn);
    void    storeNewToFile(QString fn);

    QString bErr(QString message,int s,int l);
    QString helpText(void);
    QString decimalIntToBinary(long int val);
    std::string threeSegment(const std::string& str);
    QString toPrint(Number n,int mode=0);
    int     allMatchingCount(QStringList& lst);
};

// //////////////////////////////////////////////////////////////////////// //
// Implemantations of HyperCalc,HyperCalcPrivate classes                    //
// //////////////////////////////////////////////////////////////////////// //

FuncEnvironment::FuncEnvironment(void)
{
    name = "";
    parameters.clear();
    callStack.clear();
}

HyperCalc::HyperCalc(QWidget *parent,bool welcome) :
QWidget(parent)
{
    setWindowTitle(HC_PROGRAMNAME);

    p = new HyperCalcPrivate(this);
    p->setPrecision(3);
    p->lasts = 0;

    QHBoxLayout *lay = new QHBoxLayout(this);
    p->console = new HConsolePanel(this);
    lay->addWidget(p->console);
    lay->setMargin(0);
    lay->setSpacing(0);
    resize(800,450);

    p->console->setPromptString("=");
    p->console->setMarginText(" v%0s ");
    p->console->setColor("margin",QColor(100,100,100));
    p->console->setColor("marginbg",QColor(40,40,40));
    p->console->setColor("cmdtext",Qt::yellow);
    p->console->setColor("cursor",Qt::white);
    p->console->setTextTypeColor(HC_TYPE_MESS ,QColor(210,210,210)); //messages
    p->console->setTextTypeColor(HC_TYPE_CALC ,QColor(100,188,255)); //entered calculations
    p->console->setTextTypeColor(HC_TYPE_RES  ,QColor(0,255,0)); //results
    p->console->setTextTypeColor(HC_TYPE_ERROR,QColor(255,0,0)); //errors
    p->console->setTextTypeColor(HC_TYPE_CMD  ,Qt::yellow); //commands
    p->console->setTextTypeColor(HC_TYPE_DEBUG,QColor(90,90,90)); //commands
    if(welcome)
        p->console->addText(tr("Welcome in HyperCalc"),HC_TYPE_MESS);

    connect(p->console,SIGNAL(commandEntered(QString)),this,SLOT(calcString(QString)));
    connect(p->console,SIGNAL(tabPressed(QString)),this,SLOT(hintNeeded(QString)));

    p->ufunctions.clear();

    p->functions.clear();
    p->functions["abs"] = &::hc_abs;
    p->describeFuncOutM["abs"] = tr("Absolute value");

    p->functions["round"] = &::hc_round;
    p->describeFuncOutM["round"] = tr("Round to integral value, regardless of rounding direction");

    p->functions["floor"] = &::hc_floor;
    p->describeFuncOutM["floor"] = tr("Round to largest integral value not greater than x");

    p->functions["ceil"] = &::hc_ceil;
    p->describeFuncOutM["ceil"] = tr("Round to smallest integral value not less than x");

    p->functions["sqrt"] = &::hc_sqrt;
    p->describeFuncOutM["sqrt"] = tr("Square root");

    p->functions["torad"] = &::hc_torad;
    p->describeFuncOutM["torad"] = tr("Calculate radian from degree");

    p->functions["todeg"] = &::hc_todeg;
    p->describeFuncOutM["todeg"] = tr("Calculate degree from radian");

    p->functions["sin"] = &::hc_sin;
    p->describeFuncOutM["sin"] = tr("Calculate sine of value in degree");

    p->functions["cos"] = &::hc_cos;
    p->describeFuncOutM["cos"] = tr("Calculate cosine of value in degree");

    p->functions["tan"] = &::hc_tan;
    p->describeFuncOutM["tan"] = tr("Calculate tangent of value in degree");

    p->functions["asin"] = &::hc_asin;
    p->describeFuncOutM["asin"] = tr("Calculate arc sine (degree)");

    p->functions["acos"] = &::hc_acos;
    p->describeFuncOutM["acos"] = tr("Calculate arc cosine (degree)");

    p->functions["atan"] = &::hc_atan;
    p->describeFuncOutM["atan"] = tr("Calculate arc tangent (degree)");

    p->functions["exp"] = &::hc_exp;
    p->describeFuncOutM["exp"] = tr("Exponential (e base)");

    p->functions["log"] = &::hc_log;
    p->describeFuncOutM["log"] = tr("Natural logarithm (e base)");

    p->functions["log2"] = &::hc_log2;
    p->describeFuncOutM["log2"] = tr("Logarithm to base 2");

    p->functions["log10"] = &::hc_log10;
    p->describeFuncOutM["log10"] = tr("Logarithm to base 10");

    p->functions["fact"] = &::hc_fact;
    p->describeFuncOutM["fact"] = tr("Factorial");

    p->outmods.clear();
    p->outmods.push_back("hex");
    p->describeFuncOutM["hex"] = tr("Print result in hexadecimal (RADIX 16)");
    p->outmods.push_back("oct");
    p->describeFuncOutM["oct"] = tr("Print result in octal (RADIX 8)");
    p->outmods.push_back("bin");
    p->describeFuncOutM["bin"] = tr("Print result in binary (RADIX 2)");
    p->outmods.push_back("s");
    p->describeFuncOutM["s"] = tr("Print result without three segmentation");

    p->commands.clear();
    p->commands.push_back("help");
    p->commands.push_back("exit");
    p->commands.push_back("clear");
    p->commands.push_back("precision");
    p->commands.push_back("functionlist");
    p->commands.push_back("functions");
    p->commands.push_back("outmodlist");
    p->commands.push_back("outmods");
    p->commands.push_back("version");
    p->commands.push_back("define");
    p->commands.push_back("showfunction");
    p->commands.push_back("load");
    p->commands.push_back("storenewdef");
    p->commands.push_back("listlib");

    calculated_pi = boost::math::constants::pi<boost::multiprecision::number<mp_backend, boost::multiprecision::et_off> >();

    p->defineUserFunction(QString("logX(base,x)=log(x)/log(base) #%1").arg(tr("Logarithm to arbitrary base")),tr("Internal"),true);
}

HyperCalcPrivate::HyperCalcPrivate(HyperCalc *parent)
{
    pp = parent;
}

void HyperCalc::calcString(QString str)
{
    p->calcString(str);
}

void HyperCalc::hintNeeded(QString str)
{
    p->hintNeeded(str);
}

HConsolePanel *HyperCalc::console(void)
{
    return p->console;
}

void HyperCalcPrivate::calcString(QString str)
{
    int i;
    QString original;
    original = str;
    str = str.simplified();

    //Handle commands
    if(str == "exit")
        pp->close();
    if(str == "clear")
    {
        console->clearText();
        return;
    }
    if(str.isEmpty())
    {
        console->addText("");
        return;
    }
    if(str == "functionlist")
    {
        console->addText(original,HC_TYPE_CMD);
        QStringList f = functions.keys();
        QList<UserFunction>::iterator ufi;
        for(ufi=ufunctions.begin();ufi != ufunctions.end();++ufi)
            f += ufi->name;
        f.sort();
        console->addText(QString("%1: ").arg(QObject::tr("Available functions")) +
                            f.join(","),HC_TYPE_MESS);
        return;
    }
    if(str == "functions")
    {
        console->addText(original,HC_TYPE_CMD);
        QStringList f = functions.keys();
        QList<UserFunction>::iterator ufi;
        for(ufi=ufunctions.begin();ufi != ufunctions.end();++ufi)
            f += ufi->name;
        f.sort();
        QStringList::iterator i;
        console->addText(QObject::tr("Available functions") + QString(":"),HC_TYPE_MESS);
        for(i=f.begin();i!=f.end();++i)
        {
            if(functions.contains(*i))
                console->addText(QString("%1(x) - %2").arg(*i).arg(describeFuncOutM[*i]),HC_TYPE_MESS);
            else
            {
                for(ufi=ufunctions.begin();ufi != ufunctions.end();++ufi)
                    if(ufi->name == *i)
                        break;
                if(ufi != ufunctions.end())
                {
                    console->addText(QString("%1(%2) - %3")
                                 .arg(ufi->name)
                                 .arg(ufi->parameters.join(","))
                                 .arg(ufi->comment.isEmpty() ? QObject::tr("(No description)") : ufi->comment),HC_TYPE_MESS);
                }
            }
        }
        return;
    }
    if(str == "outmodlist")
    {
        console->addText(original,HC_TYPE_CMD);
        console->addText(QString("%1: ").arg(QObject::tr("Available output modifiers")) +
                            QStringList(outmods).join(","),HC_TYPE_MESS);
        return;
    }
    if(str == "outmods")
    {
        console->addText(original,HC_TYPE_CMD);
        console->addText(QObject::tr("Available output modifiers") + QString(":"),HC_TYPE_MESS);
        QStringList::iterator i;
        for(i=outmods.begin();i!=outmods.end();++i)
            console->addText(QString("%1 - %2").arg(*i).arg(describeFuncOutM[*i])
                                ,HC_TYPE_MESS);
        return;
    }
    if(str == "version")
    {
        console->addText(original,HC_TYPE_CMD);
        console->addText(QString("%1 %2: %3\n%4: %5 (%6)\n%7: GPL\nhttp://hyperprog.com/hypercalc")
                            .arg(HC_PROGRAMNAME)
                            .arg(QObject::tr("version"))
                            .arg(HC_VERSION)
                            .arg(QObject::tr("Author"))
                            .arg("Peter Deak")
                            .arg("hyper80@gmail.com")
                            .arg(QObject::tr("License")) ,HC_TYPE_MESS);
        return;
    }
    if(str == "precision")
    {
        console->addText(original,HC_TYPE_CMD);
        console->addText(QString(QObject::tr("The precision is %1 decimal")).arg(precision()),HC_TYPE_MESS);
        return;
    }
    if(str.startsWith("precision "))
    {
        int newMaxFP;
        bool ok;

        console->addText(original,HC_TYPE_CMD);
        newMaxFP = str.mid(10,-1).toInt(&ok);
        if(!ok)
        {
            console->addText(QObject::tr("Error, cannot interpret the precision number."),HC_TYPE_ERROR);
            return;
        }
        if(newMaxFP < 0  || newMaxFP > 100)
        {
            console->addText(QString("%1 %2")
                                .arg(QObject::tr("Error, precision is out of range."))
                                .arg("( 0 <= prec <= 100 )"),HC_TYPE_ERROR);
            return;
        }
        setPrecision(newMaxFP);
        console->addText(QString(QObject::tr("New precision is %1 decimal")).arg(precision()),HC_TYPE_MESS);
        return;
    }
    if(str == "help")
    {
        console->addText(original,HC_TYPE_CMD);
        console->addText(helpText(),HC_TYPE_MESS);
        return;
    }
    if(str.startsWith("define "))
    {
        console->addText(original,HC_TYPE_CMD);
        defineUserFunction(str.mid(QString("define ").length(),-1));
        return;
    }
    if(str.startsWith("showfunction "))
    {
        console->addText(original,HC_TYPE_CMD);
        QString qn = str.mid(13,-1);
        if(qn.endsWith("()"))
            qn.replace("()","");

        if(functions.contains(qn))
        {
            console->addText(QString("%1: %2(x)\n%3: %4\n%5: %6")
                             .arg(QObject::tr("Function"))
                             .arg(qn)
                             .arg(QObject::tr("Type"))
                             .arg(QObject::tr("Native"))
                             .arg(QObject::tr("Explain text"))
                             .arg(describeFuncOutM[qn])
                             ,HC_TYPE_MESS);
            return;
        }

        QList<UserFunction>::iterator ufi;
        for(ufi=ufunctions.begin();ufi != ufunctions.end();++ufi)
            if(ufi->name == qn)
                break;
        if(ufi != ufunctions.end())
        {
            console->addText(QString("%1: %2(%3)\n%4: %5\n%6: %7\n%8: %9\n%10: %11")
                             .arg(QObject::tr("Function"))
                             .arg(ufi->name)
                             .arg(ufi->parameters.join(","))
                             .arg(QObject::tr("Type"))
                             .arg(QObject::tr("User defined"))
                             .arg(QObject::tr("Expression body"))
                             .arg(ufi->body)
                             .arg(QObject::tr("Explain text"))
                             .arg(ufi->comment)
                             .arg(QObject::tr("Source"))
                             .arg(ufi->libname.isEmpty() ? QObject::tr("Ad-hoc defined (No lib)") : ufi->libname)
                             ,HC_TYPE_MESS);
            return;
        }
        console->addText(QObject::tr("Unknown function"),HC_TYPE_ERROR);
        return;
    }
    if(str == "listlib")
    {
        console->addText(original,HC_TYPE_CMD);
        QDir d=QDir::current();
        QStringList fl = d.entryList(QString("*.def").split(";"),QDir::Files|QDir::Readable);
        console->addText(QObject::tr("Loadable function libraries:"),HC_TYPE_MESS);
        QStringList::iterator i;
        for(i=fl.begin();i!=fl.end();++i)
        {
            QString lname = *i;
            console->addText(QString(" %1").arg(lname.replace(QRegExp("\\.def$"),"")),HC_TYPE_MESS);
        }
        return;
    }
    if(str == "load *")
    {
        console->addText(original,HC_TYPE_CMD);
        QDir d=QDir::current();
        QStringList fl = d.entryList(QString("*.def").split(";"),QDir::Files|QDir::Readable);
        QStringList::iterator i;
        for(i=fl.begin();i!=fl.end();++i)
            loadFile(*i);
        return;
    }
    if(str.startsWith("load "))
    {
        console->addText(original,HC_TYPE_CMD);
        QString fn = str.mid(5,-1);
        if(!fn.endsWith(".def"))
            fn.append(".def");
        loadFile(fn);
        return;
    }
    if(str.startsWith("storenewdef "))
    {
        console->addText(original,HC_TYPE_CMD);
        QString fn = str.mid(12,-1);
        if(!fn.endsWith(".def"))
            fn.append(".def");
        storeNewToFile(fn);
        return;
    }

    //Preparing calculation
    outputModifier = "";
    Number result;

    //Remove comment
    i = str.indexOf("#");
    if(i != -1)
        str = str.mid(0,i);

    //Calculation
    try
    {
        FuncEnvironment startenv;
        result = eval(str,0,0,startenv);
    }
    catch(QString& errormsg)
    {
        console->addText(str,HC_TYPE_CALC);
        console->addText(errormsg,HC_TYPE_ERROR);
        return;
    }

    //Show the result
    console->addText(original,HC_TYPE_CALC);
    if(outputModifier == "hex")
    {
        if(boost::multiprecision::abs(result) > 2000000000.0)
        {
            console->addText(QObject::tr("Too large number to display this way."),HC_TYPE_ERROR);
            return;
        }

        double dr = boost::multiprecision::round(result).convert_to<double>();
        bool n = dr < 0 ? true : false;
        console->addText(QString()
                  .sprintf("%s%Xh",n ? "-":"",(unsigned int)(dr * (n?-1.0:1.0))),HC_TYPE_RES);
        lasts = console->maxSerial();
        return;
    }
    if(outputModifier == "oct")
    {
        if(fabs(result) > 2000000000.0)
        {
            console->addText(QObject::tr("Too large number to display this way."),HC_TYPE_ERROR);
            return;
        }
        double dr = boost::multiprecision::round(result).convert_to<double>();
        bool n = dr < 0 ? true : false;
        console->addText(QString()
                  .sprintf("%s%oo",n ? "-":"",(unsigned int)(dr * (n?-1.0:1.0))),HC_TYPE_RES);
        lasts = console->maxSerial();
        return;
    }
    if(outputModifier == "bin")
    {
        if(fabs(result) > 2000000000.0)
        {
            console->addText(QObject::tr("Too large number to display this way."),HC_TYPE_ERROR);
            return;
        }
        double dr = boost::multiprecision::round(result).convert_to<double>();
        bool n = dr < 0 ? true : false;
        console->addText(QString("%1%2b")
                            .arg(n ? "-":"")
                            .arg(decimalIntToBinary((long int)(dr * (n?-1.0:1.0))))
                ,HC_TYPE_RES);
        lasts = console->maxSerial();
        return;
    }

    console->addText(toPrint(result,outputModifier == "s" ? 0 : 1),HC_TYPE_RES);
    lasts = console->maxSerial();
}

int HyperCalcPrivate::cInS(QString str,QString chars,bool left_to_right)
{
    int i,c,l=0;
    i = left_to_right ? 0 : (str.length()-1);
    while( (left_to_right && i < str.length()) ||
           (!left_to_right && i >= 0) )
    {
        if(l == 0)
        {
            for(c=0;c<chars.length();++c)
                if(str.at(i) == chars.at(c))
                {
                    if(chars.at(c) == '-')
                    {
                        if(i < 1)
                            continue;
                        int ii=1;
                        while(i-ii > 0 && str.at(i-ii).isSpace())
                            ++ii;
                        if( str.at(i-ii) == '^' ||
                            str.at(i-ii) == '+' ||
                            str.at(i-ii) == '*' ||
                            str.at(i-ii) == '/' ||
                            str.at(i-ii) == '-'   )
                                continue;
                    }
                    return i;
                }
        }
        if(str.at(i) == '(')
            ++l;
        if(str.at(i) == ')')
            --l;
        if((left_to_right && l<0) || (!left_to_right && l>0))
            throw QString(QObject::tr("Parentheses error"));

        i = i + (left_to_right ? 1 : -1);
    }
    if(l != 0)
        throw QString(QObject::tr("Parentheses error"));

    return -1;
}

Number HyperCalcPrivate::eval(QString str,int level,int spos,FuncEnvironment& env)
{
    int p;
    Number r;
    int olen = str.length();
    str = str.simplified();

    //DEBUG console->addText(QString("%1%2").arg(QString().fill(' ',spos)).arg(str),HC_TYPE_DEBUG);

    if(str.isEmpty())
        throw QString("%1~^~\n%2")
                    .arg(QString().fill(' ',spos > 0 ? spos-1 : 0))
                    .arg(QObject::tr("Syntax error"));

    if((p=cInS(str,"-+",false)) != -1)
    {
        if(str.at(p) == '+')
            return eval(str.left(p),level+1,spos,env) + eval(str.mid(p+1,-1),level+1,spos+p+1,env);
        if(str.at(p) == '-')
            return eval(str.left(p),level+1,spos,env) - eval(str.mid(p+1,-1),level+1,spos+p+1,env);
    }

    if((p=cInS(str,"*/%",false)) != -1)
    {
        if(str.at(p) == '*')
            return eval(str.left(p),level+1,spos,env) * eval(str.mid(p+1,-1),level+1,spos+p+1,env);
        if(str.at(p) == '/')
        {
            Number v1=eval(str.left(p),level+1,spos,env);
            Number v2=eval(str.mid(p+1,-1),level+1,spos+p+1,env);
            if(v2 == 0.0)
                throw bErr(QObject::tr("Divide by zero"),spos+p+1,str.mid(p+1,-1).length());
            return (v1/v2);
        }

        if(str.at(p) == '%')
            return boost::multiprecision::fmod(eval(str.left(p),level+1,spos,env),eval(str.mid(p+1,-1),level+1,spos+p+1,env));
    }

    if((p=cInS(str,"^",false)) != -1)
    {
        Number p1 = eval(str.left(p),level+1,spos,env);
        Number p2 = eval(str.mid(p+1,-1),level+1,spos+p+1,env);
        if(p2>0.0 && p2 < 1.0 && p1 < 0.0)
            throw bErr(QObject::tr("Cannot calculate root of negative number"),spos,olen);
        return boost::multiprecision::pow(p1,p2);
    }

    if(str.simplified().startsWith("(") && str.simplified().endsWith(")"))
        return eval(str.mid(1,str.length()-2),level+1,spos+1,env);

    //internal functions
    QString fn;
    QList<QString> f = functions.keys();
    QList<QString>::iterator fit = f.begin();
    while(fit != f.end())
    {
        fn = *fit;
        if(str.startsWith(fn + "(") && str.endsWith(")"))
        {
            Number v;
            v = eval(str.mid(fn.length()+1,str.length()-fn.length()-2),level+1,spos+fn.length()+1,env);
            Number (*cfunc)(Number);
            cfunc = functions[fn];
            return (*cfunc)(v);
        }
        ++fit;
    }

    //user defined functions
    QList<UserFunction>::iterator ufi = ufunctions.begin();
    while(ufi != ufunctions.end())
    {
        if(str.startsWith(ufi->name + "(") && str.endsWith(")"))
        {
            Number v;
            //Loop protection
            if(env.callStack.contains(ufi->name))
                throw bErr(QObject::tr("The expression contains recursion"),spos,olen);

            //create environment for the function
            FuncEnvironment calledFuncEnv;
            calledFuncEnv.name=ufi->name;
            calledFuncEnv.parameters = env.parameters;
            calledFuncEnv.callStack = env.callStack;
            makeFuncEnv(level,spos,str,ufi->name,env,calledFuncEnv);

            try
            {
                v = eval(ufi->body,level+1,0,calledFuncEnv);
            }
            catch(QString& subErrorMessage)
            {
                throw bErr(QString(QObject::tr("Error in function: %1\n%2\n%3"))
                            .arg(ufi->name+QString("(%1)").arg(ufi->parameters.join(",")))
                            .arg(ufi->body)
                            .arg(subErrorMessage),spos,olen);
            }

            //Reset the environment back to the current
            return v;
        }
        ++ufi;
    }

    //output modifiers
    QList<QString>::iterator omit = outmods.begin();
    while(omit != outmods.end())
    {
        QString fn = *omit;
        if(str.startsWith(fn + "(") && str.endsWith(")"))
        {
            if(level == 0)
            {
                outputModifier = fn;
                return eval(str.mid(fn.length()+1,str.length()-fn.length()-2),level+1,spos+fn.length()+1,env);
            }
            throw bErr(QObject::tr("Output modifier only valid at toplevel"),spos,olen);
        }
        ++omit;
    }

    //constans
    if(str == "PI")
        return calculated_pi;
    if(str == "e")
    {
        return boost::multiprecision::exp(Number(1));
    }
    if(str == "LAST")
    {
        QString sub = console->lineBySerial(lasts,true,false);
        if(sub.isEmpty())
            return 0.0;
        try
        {
            FuncEnvironment emptyenv;
            return eval(sub.replace(" ",""),0,0,emptyenv);
        }
        catch(QString& errstr)
        {
            throw bErr(QObject::tr("Error evaluating line"),spos,olen);
        }
    }

    if(str.startsWith("-"))
    {
        return eval(str.mid(1,-1),level+1,spos+1,env) * -1;
    }

    //Parameter substitution if we are in function body
    if(!env.parameters.isEmpty())
    {
        if(env.parameters.keys().contains(str))
        {
            return env.parameters[str];
        }
    }

    bool ok;

    if(str.startsWith("v"))
    {
        int l;
        l = str.mid(1,-1).toInt(&ok);
        if(ok && l > 0 && l <= console->maxSerial() + 1)
        {
            QString sub = console->lineBySerial(l-1,true,false);
            if(sub.isEmpty())
                return 0.0;
            try
            {
                FuncEnvironment emptyenv;
                return eval(sub.replace(" ",""),0,0,emptyenv);
            }
            catch(QString& errstr)
            {
                throw bErr(QObject::tr("Error evaluating line"),spos,olen);
            }
        }
    }

    //numbers with different radix
    if(str.endsWith("h"))
    {
        str.truncate(str.length()-1);
        r = str.toInt(&ok,16);
        if(!ok)
            throw bErr(QObject::tr("Wrong hexadecimal number"),spos,olen);
        return Number(r);
    }
    if(str.endsWith("o"))
    {
        str.truncate(str.length()-1);
        r = str.toInt(&ok,8);
        if(!ok)
            throw bErr(QObject::tr("Wrong octal number"),spos,olen);
        return Number(r);
    }
    if(str.endsWith("b"))
    {
        str.truncate(str.length()-1);
        r = str.toInt(&ok,2);
        if(!ok)
            throw bErr(QObject::tr("Wrong binary number"),spos,olen);
        return Number(r);
    }

    //parse number from string
    try
    {
        r = Number(str.toLatin1().constData());
    }
    catch(std::exception const& e)
    {
        throw bErr(QObject::tr("Syntax error"),spos,olen);
        return 0;
    }

    return r;
}

void HyperCalcPrivate::makeFuncEnv(int level,int spos,QString callstring,QString fnName,FuncEnvironment& env,FuncEnvironment& newEnv)
{
    int skiplength = fnName.length()+1;
    QString pall = callstring.mid(skiplength,callstring.length()-fnName.length()-2);
    bool found = false;
    FuncEnvironment fe;
    UserFunction uf;
    QList<UserFunction>::iterator ufi = ufunctions.begin();
    while(ufi != ufunctions.end())
    {
        if(ufi->name == fnName)
        {
            found = true;
            uf = *ufi;
            break;
        }
        ++ufi;
    }
    if(!found)
        throw bErr(QObject::tr("Cannot exec function"),spos,callstring.length());

    fe.name = fnName;
    fe.parameters.clear();
    int parCount=0,prevPos=0,i=0,l=0;
    while( i < pall.length() )
    {
        if(l == 0 && pall.at(i) == ',')
        {
            if(parCount >= uf.parameters.count())
                throw bErr(QObject::tr("Too much parameter")+QString(": %1(%2)")
                           .arg(ufi->name)
                           .arg(ufi->parameters.join(","))
                           ,spos+skiplength,callstring.length()-skiplength-1);
            QString parname=uf.parameters.at(parCount);
            QString parval = pall.mid(prevPos,i-prevPos);
            if(parval.isEmpty())
                throw bErr(QObject::tr("Empty parameter"),spos+skiplength+prevPos,1);
            newEnv.parameters[parname] = eval(parval,level+1,spos+skiplength+prevPos,env);
            prevPos = i+1;
            ++parCount;
        }
        if(pall.at(i) == '(')
            ++l;
        if(pall.at(i) == ')')
            --l;
        if(l<0)
            throw bErr(QObject::tr("Parentheses error"),spos+skiplength,callstring.length()-skiplength-1);
        ++i;
    }

    if(l != 0)
        throw bErr(QObject::tr("Parentheses error"),spos+skiplength,callstring.length()-skiplength-1);

    if(i>prevPos)
    {
        if(parCount >= uf.parameters.count())
            throw bErr(QObject::tr("Too much parameter")+QString(": %1(%2)")
                       .arg(ufi->name)
                       .arg(ufi->parameters.join(","))
                       ,spos+skiplength,callstring.length()-skiplength-1);
        QString parname=uf.parameters.at(parCount);
        QString parval = pall.mid(prevPos,-1);
        if(parval.isEmpty())
            throw bErr(QObject::tr("Empty parameter"),spos+skiplength+prevPos,1);
        newEnv.parameters[parname] = eval(parval,level+1,spos+skiplength+prevPos,env);
        ++parCount;
    }

    newEnv.callStack += fnName;

    if(parCount < uf.parameters.count())
        throw bErr(QObject::tr("Missing parameter")+QString(": %1(%2)")
                   .arg(ufi->name)
                   .arg(ufi->parameters.join(","))
                   ,spos,callstring.length());
}

void HyperCalcPrivate::hintNeeded(QString str)
{
    QString untilCursor = str.mid(0,console->cursorPosition());
    QString afterCursor = str.mid(console->cursorPosition(),-1);

    if(untilCursor.isEmpty())
        return;

    //Searching the partially entered function name
    QChar c;
    int idx = untilCursor.length()-1;
    while(idx > 0)
    {
        c = untilCursor.at(idx);
        if(!c.isLetterOrNumber() && c != '_')
        {
            ++idx;
            if(idx >= untilCursor.length())
                return;
            break;
        }
        --idx;
    }
    if(idx<0)
        idx = 0;
    QString fpart = untilCursor.mid(idx,-1);

    //Seaching the matched functions/outmods
    int mnum=0;
    QString cmd;
    QStringList pool;
    QStringList hints;

    //Collecting possible commands,functions,etc in pool
    pool += functions.keys();
    if(idx == 0)
    {
        pool += outmods;
        pool += commands;
    }
    QList<UserFunction>::iterator ufi;
    for(ufi=ufunctions.begin();ufi != ufunctions.end();++ufi)
        pool += ufi->name;

    QList<QString>::iterator i;
    for(i = pool.begin();i != pool.end();++i)
        if(i->startsWith(fpart))
        {
            ++mnum;
            cmd = *i;
            hints.push_back(cmd);
        }

    //I found one matched, do completion (to full)
    if(mnum == 1)
    {
        int currC = console->cursorPosition();
        if(idx == 0 && commands.contains(cmd) && afterCursor.isEmpty())
        {
            console->setCommandLineText(untilCursor.mid(0,idx) + cmd);
        }
        else
        {
            console->setCommandLineText(untilCursor.mid(0,idx) + cmd + QString("()") + afterCursor);
            console->setCursorPosition(currC + cmd.length() - fpart.length() + 1);
        }
        return;
    }
    //I found more mathing, print matches or optionally do partial completion if possible
    if(mnum > 1)
    {
        //Searching common matching part
        int partialMatchingCount = allMatchingCount(hints);
        if(fpart.length() < partialMatchingCount)
        {
            QString partialliMatchedPart = cmd.mid(0,partialMatchingCount);
            int currC = console->cursorPosition();
            console->setCommandLineText(untilCursor.mid(0,idx) + partialliMatchedPart + afterCursor);
            console->setCursorPosition(currC + partialliMatchedPart.length() - fpart.length());
        }

        //print out hints
        console->addText("",DCONSOLE_TYPE_MESSAGE);
        for(i = hints.begin();i != hints.end();++i)
            console->addText(QString("%1").arg(*i),DCONSOLE_TYPE_MESSAGE);
    }
}

/*Returns the count of the characters which are same in the begin of all strings in the list */
int HyperCalcPrivate::allMatchingCount(QStringList& lst)
{
    int i,opm = -1,pm;
    if(lst.isEmpty())
        return 0;

    for(pm=0;pm<lst.at(0).length();++pm)
    {
        for(i=1;i<lst.count();++i)
            if(lst.at(i).at(pm) != lst.at(0).at(pm))
                return opm + 1;
        opm = pm;
    }
    return opm + 1;
}

QString HyperCalcPrivate::bErr(QString message,int s,int l)
{
    return QString("%1%2\n%3")
                        .arg(QString().fill(' ',s))
                        .arg(QString().fill('^',l))
                        .arg(message);
}

void HyperCalcPrivate::defineUserFunction(QString str,QString libname,bool disableNotification)
{
    QString definePart,bodyPart,commentPart;
    QString nstr=str.trimmed();

    commentPart = "";
    int hcp = str.indexOf("#");
    if(hcp != -1)
    {
        nstr = str.mid(0,hcp).simplified();
        commentPart = str.mid(hcp+1,-1).trimmed();
    }
    int eqp = nstr.indexOf("=");
    if(eqp == -1)
    {
        console->addText(QObject::tr("Syntax error"),HC_TYPE_ERROR);
        console->addText(QObject::tr("Example: define add(a,b)=a+b"),HC_TYPE_ERROR);
        return;
    }
    definePart = nstr.mid(0,eqp).simplified();
    bodyPart = nstr.mid(eqp+1,-1).simplified();
    QRegExp rex("^\\s*([\\w]+)\\(([a-z,\\s]*)\\)\\s*$");
    if(!rex.exactMatch(definePart))
    {
        console->addText(QObject::tr("Syntax error in define part"),HC_TYPE_ERROR);
        console->addText(QObject::tr("-The function name consists letters, numbers and _"),HC_TYPE_ERROR);
        console->addText(QObject::tr("-The parameters consists lowercase letters"),HC_TYPE_ERROR);
        console->addText(QObject::tr("Example: define add(a,b)=a+b"),HC_TYPE_ERROR);
        return;
    }

    UserFunction newUserFunc;
    newUserFunc.name = rex.cap(1);
    newUserFunc.body = bodyPart;
    newUserFunc.comment = commentPart;
    newUserFunc.parameters.clear();
    newUserFunc.libname = libname;
    if(!rex.cap(2).replace(QRegExp("\\s+"),"").isEmpty())
        newUserFunc.parameters = rex.cap(2).replace(QRegExp("\\s+"),"").split(',');
    if(newUserFunc.parameters.contains("e"))
    {
        console->addText(QObject::tr("Disabled parameter name")+QString(": e"),HC_TYPE_ERROR);
        return;
    }

    QStringList disabledNames;
    disabledNames += functions.keys();
    disabledNames += outmods;
    disabledNames += commands;
    disabledNames += QString("e,PI").split(",");
    if(disabledNames.contains(newUserFunc.name))
    {
        console->addText(QObject::tr("Disabled function name"),HC_TYPE_ERROR);
        return;
    }

    QList<UserFunction>::iterator ufi = ufunctions.begin();
    while(ufi != ufunctions.end())
    {
        if(ufi->name == newUserFunc.name)
        {
            QList<UserFunction>::iterator to_delete = ufi;
            ++ufi;
            ufunctions.erase(to_delete);
        }
        else
            ++ufi;
    }

    ufunctions.push_back(newUserFunc);
    if(!disableNotification)
        console->addText(QObject::tr("Function defined."),HC_TYPE_MESS);
}

void HyperCalcPrivate::loadFile(QString fn)
{
    QFile definitionFile(fn);
    if(!definitionFile.exists() || !definitionFile.open(QIODevice::ReadOnly))
    {
        console->addText(QObject::tr("Cannot open definition file"),HC_TYPE_ERROR);
        return;
    }

    QStringList lines = QString(definitionFile.readAll()).split("\n");
    QStringList::iterator i;
    for(i=lines.begin();i!=lines.end();++i)
    {
        QString def = *i;
        def = def.simplified();
        if(def.startsWith("#"))
            continue;
        if(def.startsWith("define "))
        {
            QString dstr = def.mid(QString("define ").length(),-1);
            if(!dstr.isEmpty())
                defineUserFunction(dstr,fn.replace(QRegExp("\\.def$"),""),true);
        }
    }
    definitionFile.close();
}

void HyperCalcPrivate::storeNewToFile(QString fn)
{
    QFile definitionFile(fn);

    int c=0;
    QList<UserFunction>::iterator ufi;
    for(ufi=ufunctions.begin();ufi != ufunctions.end();++ufi)
        if(ufi->libname.isEmpty())
            ++c;
    if(c <= 0)
    {
        console->addText(QObject::tr("There is no new defined function"),HC_TYPE_ERROR);
        return;
    }

    if(definitionFile.exists())
    {
        console->addText(QObject::tr("The file has already exists"),HC_TYPE_ERROR);
        return;
    }
    if(!definitionFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        console->addText(QObject::tr("Cannot open file fow writing"),HC_TYPE_ERROR);
        return;
    }

    QTextStream out(&definitionFile);
    out << "#Generated from HyperCalc\n";
    for(ufi=ufunctions.begin();ufi != ufunctions.end();++ufi)
        if(ufi->libname.isEmpty())
        {
            out << "define " << ufi->name << "(" <<ufi->parameters.join(",") << ")=" <<
                   ufi->body << " #" << ufi->comment << "\n";
            ufi->libname = fn.replace(QRegExp("\\.def$"),"");
        }
    definitionFile.close();
}

QString HyperCalcPrivate::decimalIntToBinary(long int val)
{
    QString bin="";

    while(val != 0)
    {
        if(val%2 == 0)
            bin.push_front("0");
        else
        {
            bin.push_front("1");
            --val;
        }
        val = val / 2;
    }
    return bin;
}

QString HyperCalcPrivate::helpText(void)
{
    return QString("%1\n%2: %3\n%4: %5\n%6:\n")
            .arg(QObject::tr("HyperCalc high precision calculator console"))
            .arg(QObject::tr("Available operators"))
            .arg("+ - * / % ^")
            .arg(QObject::tr("Available constant/variables"))
            .arg("vX LAST PI e")
            .arg(QObject::tr("Available commands"))
           +
           QString(" help - %1\n").arg(QObject::tr("Print this help"))
           +
           QString(" exit - %1\n").arg(QObject::tr("Close the HyperCalc"))
           +
           QString(" clear - %1\n").arg(QObject::tr("Clear the window"))
           +
           QString(" precision - %1\n").arg(QObject::tr("Print the maximum number of fragment letters of a decimal numbers"))
           +
           QString(" precision <n> - %1\n").arg(QObject::tr("Set the maximum number of fragment letters of a decimal numbers to n"))
           +
           QString(" functionlist - %1\n").arg(QObject::tr("List the available functions"))
           +
           QString(" functions - %1\n").arg(QObject::tr("List the available functions with explanation"))
           +
           QString(" outmodlist - %1\n").arg(QObject::tr("List the available output modifiers"))
           +
           QString(" outmods - %1\n").arg(QObject::tr("List the available output modifiers with explanation"))
           +
           QString(" define - %1\n").arg(QObject::tr("Define a new or redefine an existing function:\n"
                                                     "          define name(par1,par2,...)=expression  # Description"))
           +
           QString(" showfunction <func> - %1\n").arg(QObject::tr("Show all information about the <func> function"))
           +
           QString(" listlib - %1\n").arg(QObject::tr("List available loadable libraries"))
           +
           QString(" load <filename> - %1\n").arg(QObject::tr("Load <filename> or * function definition file"))
           +
           QString(" storenewdef <filename> - %1\n").arg(QObject::tr("Store new user defined functions to <filename>"))
           +
           QString(" version - %1\n").arg(QObject::tr("Show the current version information"));
}

HyperCalc::~HyperCalc()
{
    delete p;
}

void HyperCalcPrivate::setPrecision(int digits)
{
    maxFragmentLetters = digits;
    Number base = 10;
    omul = boost::multiprecision::pow(base,maxFragmentLetters);
}

int HyperCalcPrivate::precision(void)
{
    return maxFragmentLetters;
}

std::string HyperCalcPrivate::threeSegment(const std::string& str)
{
    std::string rs="";
    int i,r,f;
    r = str.size();
    f = str.find(".");
    if(f != (int)std::string::npos)
    {
        rs=".";
        i=0;
        r=f+1;
        while(r < (int)str.size())
        {
            rs.push_back(str.at(r));
            i++;
            if(i%3==0 && i!=0 && r!=(int)str.size()-1)
                rs.push_back(' ');
            r++;
        }
        r = f;
    }
    i=0;
    r--;
    while(r>=0)
    {
        rs.insert(0,std::string(1,str.at(r)));
        i++;
        if(i%3==0 && i!=0 && r!=0)
            rs.insert(0," ");
        r--;
    }
    return rs;
}

QString HyperCalcPrivate::toPrint(Number n,int mode)
{
    n = boost::multiprecision::round(n * omul) / omul;
    return QString::fromStdString( mode == 0 ? n.str() : threeSegment(n.str()) );
}

/*
int HyperCalc::my_dtoa(double v,char *buffer,int bufflen,int min,int max,int group)
{
    int digitnum;
    int i,forlength;
    int length=0; //the currnt filled length of the buffer

    char digit;
    char *str = buffer;

    double i_ip,i_fp,idigit_value;
    double ip,fp;

    bufflen -= 2; //decrease bufflen value, to avoid decreasing in every if

    if(isnan(v))
    {
        if(bufflen < 4)
            return 1;
        strcpy(str,"NaN");
        return 0;
    }
    if(isinf(v))
    {
        if(bufflen < 4)
            return 1;
        strcpy(str,"Inf");
        return 0;
    }

    //split the number to integer and fractional part.
    fp = fabs(modf(v,&ip));
    ip = fabs(ip);
    if(fp != 0.0)
    {
        fp *= pow(10.0,max);
        fp = floor(fp + 0.5);
    }
    i_ip=floor(ip);
    i_fp=floor(fp);

    //If the original (rounded) number is negative put the sign to front
    v *= pow(10.0,max);
    v = floor(v + 0.5);
    if (v < 0)
    {
        *(str++) = '-';
        ++length;
        v = -v;
    }

    //Generate integer part (from i_ip)
    idigit_value = 1;
    digitnum = 1;
    while(idigit_value*10 <= i_ip)
    {
        idigit_value *= 10;
        ++digitnum;
    }
    forlength=0;
    while(idigit_value >= 1)
    {
        //put grouping space if set
        if(group && forlength != 0 && digitnum % 3 == 0)
        {
            *(str++) = ' ';
            ++length;
            if(length >= bufflen)
            {
                *(str) = '\0';
                return 1;
            }
        }

        digit = static_cast<char>(floor(i_ip/idigit_value));
        i_ip = i_ip - idigit_value*(double)digit;

        *(str++) = '0' + digit%10;
        ++length;
        --digitnum;
        ++forlength;
        idigit_value /= 10;

        if(length >= bufflen)
        {
            *(str) = '\0';
            return 1;
        }
    }

    //Generate fractional part (from i_fp)
    digitnum=0;
    if( i_fp > 0 )
    {
        *(str++) = '.';
        ++length;

        idigit_value = 1;
        for(i=0;i<max-1;++i)
            idigit_value *= 10;

        while (idigit_value >= 1)
        {
            if(group && digitnum && digitnum%3 == 0)
            {
                *(str++) = ' ';
                ++length;
                if(length >= bufflen)
                {
                    *(str) = '\0';
                    return 1;
                }
            }

            digit = static_cast<char>(floor(i_fp / idigit_value));
            i_fp = i_fp-idigit_value*(double)digit;

            *(str++) = '0' + digit%10;
            ++length;
            ++digitnum;
            idigit_value /= 10;

            if(length >= bufflen)
            {
                *(str) = '\0';
                return 1;
            }

            if(digitnum >= min && i_fp == 0)
                break;
        }
    }
    else
    {   //the original number was an integer, so we fill the minimal fractional part with zeros
        if(min > 0)
        {
            *(str++) = '.';
            ++length;
            for(digitnum=0;digitnum<min;)
            {
                if(group && digitnum && digitnum%3 == 0)
                {
                    *(str++) = ' ';
                    ++length;
                    if(length >= bufflen)
                    {
                        *(str) = '\0';
                        return 1;
                    }
                }
                *(str++) = '0';
                ++length;
                ++digitnum;
                if(length >= bufflen)
                {
                    *(str) = '\0';
                    return 1;
                }
            }
        }
    }
    *str = '\0';
    return 0;
}
*/

/*
QString HyperCalc::doubleToQString(double val,int min,int max,int group)
{
    QString v;
    char buffer[128];
    my_dtoa(val,buffer,128,min,max,group);
    v = buffer;
    return v;
}
*/

//end code.
