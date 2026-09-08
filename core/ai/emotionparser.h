#pragma once

#include <QString>

class EmotionParser
{
public:
    struct Result
    {
        QString text;
        QString speechText;
        QString emotion;
    };

    static Result parse(const QString &rawText);
};
