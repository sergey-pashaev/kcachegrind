/* This file is part of KCachegrind.
   Copyright (c) 2008-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

   KCachegrind is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation, version 2.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <QCoreApplication>
#include <QTextStream>
#include <QSet>

#include <memory>
#include <iostream>
#include <cstdint>
#include <cmath>

#include "tracedata.h"
#include "loader.h"
#include "config.h"
#include "globalconfig.h"
#include "logger.h"

typedef std::shared_ptr<TraceData> TraceDataPtr;

struct FuncPair {
    FuncPair(TraceFunction* func1 = nullptr, TraceFunction* func2 = nullptr) : f1(func1), f2(func2) {}

    TraceFunction* f1;
    TraceFunction* f2;
};

struct Total {
    SubCost a;
    SubCost b;
};

typedef double SortDiffKey;
typedef uint64_t SortKey;

typedef QMultiMap<SortDiffKey, FuncPair> SortedCommonFunctionsMap;
typedef QMultiMap<SortKey, TraceFunction*> SortedFunctionsMap;

struct DiffParams {
    DiffParams()
        : eventStr("Ir"), event(nullptr), exclusive(false), perCall(false),
          sortByCallCount(false), sortByPercentage(false),
          hideTemplates(true), showCycles(false), minimalValue(1.0) /* 1% or 1 - to skip functions w/o changes */
    {}

    QString eventStr;
    EventType* event;
    bool exclusive; // = false means inclusive
    bool perCall;
    bool sortByCallCount;
    bool sortByPercentage;
    bool hideTemplates;
    bool showCycles;
    SortDiffKey minimalValue;
    QString funcNameFilter;
};

void showHelp(QTextStream& out, bool fullHelp = true)
{
    out <<  "Compare profiles from callgrind files. (c) 2017 S. Pashaev\n";

    if (!fullHelp)
        out << "Type 'cgdiff -h' for help." << endl;
    else
        out << "Usage: cgdiff [options] <file> ...\n\n"
               "Options:\n"
               " -h        Show this help text\n"
               " -n        Do not detect recursive cycles\n"
               " -m <num>  Show only function with <num> minimal delta value\n"
               " -s <ev>   Show counters for event <ev>\n"
               " -f <str>  Show only functions with <str> in name\n"
               " -e        Sort by exclusive cost change\n"
               " -c        Sort by call count change\n"
               " -r        Sort percentage change\n"
               " -p        Use per call counters\n"
               " -t        Show templates" << endl;

    exit(1);
}

TraceDataPtr loadTraceData (const QString &file) {
    auto ret = TraceDataPtr(new TraceData(new Logger));
    if (!ret->load(QStringList(file))) {
        std::cout << file.toStdString() << ": loading failure" << std::endl;
        exit(1);
    }
    return ret;
}

bool checkEventTypesEqual(const TraceDataPtr td1, const TraceDataPtr td2) {
    // todo: chech that requested event type exist in both td1 & td2
    EventTypeSet* eset1 = td1->eventTypes();
    EventTypeSet* eset2 = td2->eventTypes();

    int td1RealCount = eset1->realCount();
    int td2RealCount = eset2->realCount();

    if (td1RealCount == 0) {
        std::cout << "Error: No event types found:" << td1->fullName().toStdString() << std::endl;
        return false;
    }

    if (td2RealCount == 0) {
        std::cout << "Error: No event types found:" << td2->fullName().toStdString() << std::endl;
        return false;
    }

    int td1DerivedCount = eset1->derivedCount();
    int td2Derivedcount = eset2->derivedCount();

    if (td1RealCount != td2RealCount) {
        std::cout << "Error: different event types" << std::endl;
        return false;
    }

    if (td1DerivedCount != td2Derivedcount) {
        std::cout << "Error: different derived types" << std::endl;
        return false;
    }

    EventType* et1 = nullptr;
    EventType* et2 = nullptr;
    int realCount = td1RealCount; // or td2*, doesn't matter
    for (int i = 0; i < realCount; i++) {
        et1 = eset1->realType(i);
        et2 = eset2->realType(i);

        if (et1->longName() != et2->longName()) {
            std::cout << "Error: different event type names" << std::endl;
            return false;
        }
    }

    int derivedCount = td1DerivedCount;
    for (int i = 0; i < derivedCount; i++) {
        et1 = eset1->derivedType(i);
        et2 = eset2->derivedType(i);

        if (et1->longName() != et2->longName()) {
            std::cout << "Error: different derived type names" << std::endl;
            return false;
        }
    }

    return true;
}

int64_t diff(SubCost a, SubCost b) {
    return (int64_t)(b.v - a.v);
}

double percentDiff(SubCost a, SubCost b) {
    return (((double(b.v)) - (double(a.v))) / (double(a.v))) * 100.0;
}

SubCost perCall(SubCost cost, SubCost calls) {
    if (calls == 0) {
        return 0;
    }

    return cost / calls;
}

void printHeaderTotals(QTextStream &out) {
    out << "\nTotals for event types:\n";

    // print header
    out.setFieldWidth(14);
    out.setFieldAlignment(QTextStream::AlignRight);
    out << "A"; out << " ";

    out.setFieldWidth(14);
    out.setFieldAlignment(QTextStream::AlignRight);
    out << "B"; out << " ";

    out.setFieldWidth(14);
    out.setFieldAlignment(QTextStream::AlignRight);
    out << "B - A"; out << " ";

    out.setFieldWidth(9);
    out.setFieldAlignment(QTextStream::AlignRight);
    out << "+/- %"; out << " ";

    out.setFieldWidth(0);
    out << "Event type";

    out << endl;
}

void printRealEventTypeTotals(QTextStream &out, EventType* et, const TraceDataPtr td1, const TraceDataPtr td2) {
    SubCost cost1 = td1->subCost(et);
    SubCost cost2 = td2->subCost(et);

    out.setFieldWidth(14);
    out.setFieldAlignment(QTextStream::AlignRight);
    out << cost1;
    out << " ";

    out.setFieldWidth(14);
    out.setFieldAlignment(QTextStream::AlignRight);
    out << cost2;
    out << " ";

    out.setFieldWidth(14);
    out.setFieldAlignment(QTextStream::AlignRight);
    out << diff(cost1, cost2);
    out << " ";

    out.setFieldWidth(9);
    out.setFieldAlignment(QTextStream::AlignRight);
    QString text;
    text.sprintf("%+6.2f", percentDiff(cost1, cost2));
    out << text;
    out << " ";

    out.setFieldWidth(0);
    out << et->longName() << " (" << et->name() << ")\n";
}

void printDerivedEventTypeTotals(QTextStream &out, EventType* et, const TraceDataPtr td1, const TraceDataPtr td2) {
    SubCost cost1 = td1->subCost(et);
    SubCost cost2 = td2->subCost(et);

    out.setFieldWidth(14);
    out.setFieldAlignment(QTextStream::AlignRight);
    out << cost1;
    out << " ";

    out.setFieldWidth(14);
    out.setFieldAlignment(QTextStream::AlignRight);
    out << cost2;
    out << " ";

    out.setFieldWidth(14);
    out.setFieldAlignment(QTextStream::AlignRight);
    out << diff(cost1, cost2);
    out << " ";

    out.setFieldWidth(9);
    out.setFieldAlignment(QTextStream::AlignRight);
    QString text;
    text.sprintf("%+6.2f", percentDiff(cost1, cost2));
    out << text;
    out << " ";

    out.setFieldWidth(0);
    out << "   " << et->longName() <<
           " (" << et->name() << " = " << et->formula() << ")\n";
}

void printTotals(QTextStream &out, EventTypeSet* events, const TraceDataPtr tdata1, const TraceDataPtr tdata2) {
    for (int i = 0; i < events->realCount(); i++) {
        printRealEventTypeTotals(out, events->realType(i), tdata1, tdata2);
    }

    for (int i = 0; i < events->derivedCount(); i++) {
        printDerivedEventTypeTotals(out, events->derivedType(i), tdata1, tdata2);
    }
    out << endl;
}

void printCommonSortedBy(QTextStream &out, const DiffParams &params) {
    out << "Sorted by: ";
    if (params.sortByPercentage) {
        out << "percentage ";
    }
    out << "change in ";

    if (params.sortByCallCount) {
        out << "call count ";
    } else {
        out << (params.exclusive ? "Exclusive " : "Inclusive ");

        out << params.event->longName() << " (" << params.event->name() << ") ";

        if (params.perCall) {
            out << "(per call)";
        }

    }

    out << endl;
}

void printFilteredBy(QTextStream &out, const DiffParams &params) {
    if (params.funcNameFilter.size() || params.minimalValue > 0) {
        out << "Filtered by: ";

        if (params.funcNameFilter.size()) {
            out << "name: '" << params.funcNameFilter << "' ";
        }

        if (params.minimalValue > 0) {
            out << "minimal value: '" << params.minimalValue << "' ";
        }

        out << endl;
    }
}

void printCommonFilteredTotals(QTextStream &out, const Total &totals) {
    QString text;
    text.sprintf("%21llu %21llu %+21ld %+14.2f / %s", totals.a.v, totals.b.v, diff(totals.a, totals.b), percentDiff(totals.a, totals.b), "TOTAL");
    out << endl << text << endl;
}

void printFilteredTotals(QTextStream &out, SubCost total) {
    QString text;
    text.sprintf("                      %21llu / %s", total.v, "TOTAL");
    out << endl << text << endl;
}

void printSortedBy(QTextStream &out, const DiffParams &params) {
    out << "Sorted by: ";

    if (params.sortByCallCount) {
        out << "call count ";
    } else {
        out << (params.exclusive ? "Exclusive " : "Inclusive ");

        out << params.event->longName() << " (" << params.event->name() << ") ";

        if (params.perCall) {
            out << "(per call)";
        }

    }

    out << endl;
}

SubCost getValue(const DiffParams &params, TraceFunction* f) {
    SubCost v = params.exclusive ? f->subCost(params.event) : f->inclusive()->subCost(params.event);
    if (params.perCall) {
        v = perCall(v, f->calledCount());
    }

    return v;
}

template <typename K, typename C>
void filter(const DiffParams &params, TraceFunction* f, K sortKey, C &out) {
    if (sortKey >= params.minimalValue) {
        if (params.funcNameFilter.size()) {
            if (f->prettyName().indexOf(params.funcNameFilter) >= 0) {
                out.insert(sortKey, f);
            }
        } else {
            out.insert(sortKey, f);
        }
    }
}

void fillCommonFunctionsMap(const DiffParams &params, const QSet<QString> &keys, TraceFunctionMap &map1, TraceFunctionMap &map2, SortedCommonFunctionsMap &common, Total &total) {
    for (auto it = keys.cbegin(); it != keys.cend(); ++it) {
        //todo: use ref here
        auto key = *it;

        //todo: check with mapX.end()
        TraceFunctionMap::iterator itf1 = map1.find(key);
        TraceFunctionMap::iterator itf2 = map2.find(key);

        FuncPair fp(&(*itf1), &(*itf2));
        Q_ASSERT(fp.f1->prettyName() == fp.f2->prettyName());

        SubCost v1 = getValue(params, fp.f1);
        SubCost v2 = getValue(params, fp.f2);

        SubCost callcount1 = fp.f1->calledCount();
        SubCost callcount2 = fp.f2->calledCount();

        if (params.sortByCallCount) {
            v1 = callcount1;
            v2 = callcount2;
        }

        SortDiffKey sortKey;
        if (params.sortByPercentage) {
            sortKey = std::abs((SortDiffKey)percentDiff(v1, v2));
        } else {
            sortKey = std::abs((SortDiffKey)diff(v1, v2));
        }

        // filter(params, fp.f1, sortKey, common);
        if (sortKey >= params.minimalValue) {
            if (params.funcNameFilter.size()) {
                if (fp.f1->prettyName().indexOf(params.funcNameFilter) >= 0) {
                    total.a += v1;
                    total.b += v2;
                    common.insert(sortKey, fp);
                }
            } else {
                total.a += v1;
                total.b += v2;
                common.insert(sortKey, fp);
            }
        }
    }
}

void printCommonFunctionsHeader(QTextStream &out) {
    out << "                    A ";
    out << "                    B ";
    out << "                B - A ";
    out << "          +/-% ";
    out << "/ ";
    out << "function name";
    out << endl;
}

void printCommonFunctionsMap(QTextStream &out, const DiffParams &params, SortedCommonFunctionsMap &common) {
    // iterate backwards
    QMapIterator<SortDiffKey, FuncPair> it(common);
    it.toBack();
    while (it.hasPrevious()) {
        it.previous();

        FuncPair fp = it.value();
        Q_ASSERT(fp.f1->prettyName() == fp.f2->prettyName());

        SubCost v1 = getValue(params, fp.f1);
        SubCost v2 = getValue(params, fp.f2);

        SubCost callcount1 = fp.f1->calledCount();
        SubCost callcount2 = fp.f2->calledCount();

        int64_t diffAbs = diff(v1, v2);
        double diffPercent = percentDiff(v1, v2);
        if (params.sortByCallCount) {
            diffAbs = diff(callcount1, callcount2);
            diffPercent = percentDiff(callcount1, callcount2);
        }

        QString text;
        text.sprintf("%21llu %21llu %+21ld %+14.2f / %s", v1.v, v2.v, diffAbs, diffPercent, fp.f1->prettyName().toStdString().c_str());
        out << text << endl;
    }
}

void printFunctionsHeader(QTextStream &out) {
    out << "           call count ";
    out << "                value ";
    out << "/ ";
    out << "function name";
    out << endl;
}

void fillFunctionsMap(const DiffParams &params, const QSet<QString> &keys, TraceFunctionMap &map, SortedFunctionsMap &sorted, SubCost &total) {
    for (auto it = keys.cbegin(); it != keys.cend(); ++it) {
        //todo: use ref
        auto key = *it;

        TraceFunctionMap::iterator itf = map.find(key);
        TraceFunction* f = &(*itf);

        SubCost callcount = f->calledCount();
        SubCost v = getValue(params, f);

        SortKey sortKey = params.sortByCallCount ? callcount : v;

        // filter(params, fp.f1, sortKey, common);
        if (sortKey >= params.minimalValue) {
            if (params.funcNameFilter.size()) {
                if (f->prettyName().indexOf(params.funcNameFilter) >= 0) {
                    total += v;
                    sorted.insert(sortKey, f);
                }
            } else {
                total += v;
                sorted.insert(sortKey, f);
            }
        }
    }
}

void printFunctionsMap(QTextStream &out, const DiffParams &params, SortedFunctionsMap &map) {
    // iterate backwards
    QMapIterator<SortKey, TraceFunction*> it(map);
    it.toBack();
    while (it.hasPrevious()) {
        it.previous();

        TraceFunction* f = it.value();

        SubCost callcount = f->calledCount();
        SubCost v = getValue(params, f);

        QString text;
        text.sprintf("%21llu %21llu / %s", callcount.v, v.v, f->prettyName().toStdString().c_str());
        out << text << endl;
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    Loader::initLoaders();
    ConfigStorage::setStorage(new ConfigStorage);
    GlobalConfig::config()->addDefaultTypes();

    QStringList list = app.arguments();
    list.pop_front();
    if (list.isEmpty()) showHelp(out, false);

    DiffParams params;
    QStringList files;

    bool toDoubleOk = true;
    QString toDoubleStr;

    for(int arg = 0; arg < list.count(); arg++) {
        if      (list[arg] == QLatin1String("-h")) showHelp(out);
        else if (list[arg] == QLatin1String("-n")) params.showCycles = true;
        else if (list[arg] == QLatin1String("-m")) toDoubleStr = list[++arg];
        else if (list[arg] == QLatin1String("-s")) params.eventStr = list[++arg];
        else if (list[arg] == QLatin1String("-f")) params.funcNameFilter = list[++arg];
        else if (list[arg] == QLatin1String("-e")) params.exclusive = true;
        else if (list[arg] == QLatin1String("-c")) params.sortByCallCount = true;
        else if (list[arg] == QLatin1String("-r")) params.sortByPercentage = true;
        else if (list[arg] == QLatin1String("-p")) params.perCall = true;
        else if (list[arg] == QLatin1String("-t")) params.hideTemplates = false;
        else
            files << list[arg];
    }

    if (toDoubleStr.size()) {
        params.minimalValue = toDoubleStr.toDouble(&toDoubleOk);
        if (!toDoubleOk) {
            out << "Error: -m value:'" << toDoubleStr << "' is not floating point number." << endl;
            return 1;
        }
    }

    GlobalConfig::setHideTemplates(params.hideTemplates);
    GlobalConfig::setShowCycles(params.showCycles);

    // we expect two profile data files to compare
    Q_ASSERT(files.size() == 2);
    auto tdata1 = loadTraceData(files.at(0));
    auto tdata2 = loadTraceData(files.at(1));

    if (!checkEventTypesEqual(tdata1, tdata2)) {
        return 1;
    }

    // set event type
    EventTypeSet* events = tdata1->eventTypes();
    params.event = events->type(params.eventStr);
    if (!params.event) {
        out << "Error: event '" << params.eventStr << "' not found." << endl;
        return 1;
    }

    // print legend
    out << "A = " << files.at(0) << endl;
    out << "B = " << files.at(1) << endl;

    printHeaderTotals(out);
    printTotals(out, events, tdata1, tdata2);

    TraceFunctionMap &map1 = tdata1->functionMap();
    TraceFunctionMap &map2 = tdata2->functionMap();

    auto keys1 = map1.uniqueKeys();
    auto keys2 = map2.uniqueKeys();

    QSet<QString> intersection(keys1.toSet().intersect(keys2.toSet()));
    if (!intersection.empty()) {
        SortedCommonFunctionsMap common;
        Total total;
        fillCommonFunctionsMap(params, intersection, map1, map2, common, total);
        if (common.size()) {
            out << "Common functions:" << endl;
            printCommonSortedBy(out, params);
            printFilteredBy(out, params);
            printCommonFunctionsHeader(out);
            printCommonFunctionsMap(out, params, common);
            printCommonFilteredTotals(out, total);
            out << endl;
        }
    }

    QSet<QString> keys1minus2(keys1.toSet().subtract(keys2.toSet()));
    if (!keys1minus2.empty()) {
        SortedFunctionsMap f1minus2;
        SubCost total;
        fillFunctionsMap(params, keys1minus2, map1, f1minus2, total);
        if (f1minus2.size()) {
            out << "A functions (not in B):" << endl;
            printSortedBy(out, params);
            printFilteredBy(out, params);
            printFunctionsHeader(out);
            printFunctionsMap(out, params, f1minus2);
            printFilteredTotals(out, total);
            out << endl;
        }
    }

    QSet<QString> keys2minus1(keys2.toSet().subtract(keys1.toSet()));
    if (!keys2minus1.empty()) {
        SortedFunctionsMap f2minus1;
        SubCost total;
        fillFunctionsMap(params, keys2minus1, map2, f2minus1, total);
        if (f2minus1.size()) {
            out << "B functions (not in A):" << endl;
            printSortedBy(out, params);
            printFilteredBy(out, params);
            printFunctionsHeader(out);
            printFunctionsMap(out, params, f2minus1);
            printFilteredTotals(out, total);
            out << endl;
        }
    }

    return 0;
}
