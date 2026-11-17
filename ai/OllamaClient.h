#pragma once

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class OllamaClient final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl endpoint READ endpoint WRITE setEndpoint NOTIFY endpointChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(int timeoutMs READ timeoutMs WRITE setTimeoutMs NOTIFY timeoutMsChanged)

public:
    explicit OllamaClient(QObject *parent = nullptr);

    QUrl endpoint() const;
    QString model() const;
    int timeoutMs() const;
    void setEndpoint(const QUrl &endpoint);
    void setModel(const QString &model);
    void setTimeoutMs(int timeoutMs);

    void generate(const QString &prompt);

signals:
    void responseReady(const QString &response);
    void requestFailed(const QString &message, bool unavailable);
    void requestStarted();
    void endpointChanged();
    void modelChanged();
    void timeoutMsChanged();

private:
    void finishReply(QNetworkReply *reply);

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    QUrl m_endpoint;
    QString m_model;
    int m_timeoutMs = 180000;
    int m_contextLength = 2048;
};
