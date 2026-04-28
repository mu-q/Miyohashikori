#pragma once

#include "iaisession.h"

class NullAiSession : public IAiSession
{
    Q_OBJECT

public:
    explicit NullAiSession(QObject *parent = nullptr);

    void submit(const QString &userText) override;
};
