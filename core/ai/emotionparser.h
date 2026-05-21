#pragma once

#include <QString>

class EmotionParser
{
public:
    struct Result
    {
        QString text;
        QString emotion;
    };

    static Result parse(const QString &rawText);
};
