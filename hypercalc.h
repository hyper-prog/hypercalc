/*  HyperCalc
    High precision command line styled calculator
    http://hyperprog.com/hypercalc/

   (C) 2013 - 2019 Peter Deak  (hyper80@gmail.com)

    License: GPLv2  http://www.gnu.org/licenses/gpl-2.0.html

    hypercalc.h
*/
#ifndef HYPERCALC_H
#define HYPERCALC_H

#include <QtCore>
#include <QWidget>

#define HC_PROGRAMNAME "HyperCalc"
#define HC_VERSION     "1.22"

#define HC_TYPE_MESS   1
#define HC_TYPE_CALC   2
#define HC_TYPE_RES    3
#define HC_TYPE_ERROR  4
#define HC_TYPE_CMD    5
#define HC_TYPE_DEBUG  9

class HConsolePanel;
class HyperCalc : public QWidget
{
    Q_OBJECT
    
public:
    HyperCalc(QWidget *parent = 0,bool welcome = true);
    ~HyperCalc();

    HConsolePanel *console(void);

public slots:
    void calcString(QString str);
    void hintNeeded(QString str);

public:
    class HyperCalcPrivate *p;
};

#endif // HYPERCALC_H
