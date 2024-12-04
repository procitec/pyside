// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifndef STDSHAREDPTRTESTBENCH_H
#define STDSHAREDPTRTESTBENCH_H

#include "libsmartmacros.h"

#include <memory>
#include <string>

class Integer;

class LIB_SMART_API StdSharedPtrTestBench
{
public:
    StdSharedPtrTestBench();
    ~StdSharedPtrTestBench();

    static std::shared_ptr<Integer> createInteger(int v = 42);
    static std::shared_ptr<Integer> createNullInteger();
    static void printInteger(const std::shared_ptr<Integer> &);

    static std::shared_ptr<int> createInt(int v = 42);
    static std::shared_ptr<int> createNullInt();
    static void printInt(const std::shared_ptr<int> &);

    static std::shared_ptr<double> createDouble(double v = 42);
    static std::shared_ptr<double> createNullDouble();
    static void printDouble(const std::shared_ptr<double> &);

    static std::shared_ptr<std::string> createString(const char *text);
    static std::shared_ptr<std::string> createNullString();
    static void printString(const std::shared_ptr<std::string> &);
};

class LIB_SMART_API StdSharedPtrVirtualMethodTester
{
public:
    StdSharedPtrVirtualMethodTester();
    virtual ~StdSharedPtrVirtualMethodTester();

    std::shared_ptr<Integer> callModifyInteger(const std::shared_ptr<Integer> &p);

protected:
    virtual std::shared_ptr<Integer> doModifyInteger(std::shared_ptr<Integer> p);
};

#endif // STDSHAREDPTRTESTBENCH_H
