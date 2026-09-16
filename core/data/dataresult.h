#pragma once

#include <QString>

#include <utility>

template<typename T>
struct DataResult
{
    bool success = false;
    T value{};
    QString error;

    static DataResult ok(T result)
    {
        DataResult output;
        output.success = true;
        output.value = std::move(result);
        return output;
    }

    static DataResult fail(const QString &message)
    {
        DataResult output;
        output.error = message;
        return output;
    }
};
