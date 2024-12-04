// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

/*********************************************************************
 * INJECT CODE
 ********************************************************************/

// Note: Lambdas need to be inline, QTBUG-104481
// @snippet qhttpserver-route
QString rule = %CONVERTTOCPP[QString](%PYARG_1);
auto *callable = %PYARG_2;

bool cppResult = %CPPSELF.%FUNCTION_NAME(rule,
                                         [callable](const QHttpServerRequest &request) -> QString {
    Shiboken::GilState state;
    auto *requestPtr = &request;
    Shiboken::AutoDecRef arglist(PyTuple_New(1));
    PyTuple_SetItem(arglist, 0,
                     %CONVERTTOPYTHON[QHttpServerRequest *](requestPtr));
    PyObject *ret = PyObject_CallObject(callable, arglist);
    if (PyErr_Occurred())
        PyErr_Print();
    if (ret == nullptr)
        return QString{};
    QString cppResult = %CONVERTTOCPP[QString](ret);
    return cppResult;
});

%PYARG_0 = %CONVERTTOPYTHON[bool](cppResult);
// @snippet qhttpserver-route

// @snippet qhttpserver-addafterrequesthandler
auto *callable = %PYARG_2;

auto callback = [callable](const QHttpServerRequest &request,
                           QHttpServerResponse &response) {
    Shiboken::GilState state;
    Shiboken::AutoDecRef arglist(PyTuple_New(2));
    auto *responsePtr = &response;
    auto *requestPtr = &request;
    PyTuple_SetItem(arglist, 0,
                     %CONVERTTOPYTHON[QHttpServerRequest *](requestPtr));
    PyTuple_SetItem(arglist, 1,
                     %CONVERTTOPYTHON[QHttpServerResponse *](responsePtr));
    PyObject_CallObject(callable, arglist);
    if (PyErr_Occurred())
        PyErr_Print();
};

%CPPSELF.%FUNCTION_NAME(%1, callback);
// @snippet qhttpserver-addafterrequesthandler
