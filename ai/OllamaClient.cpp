#include "OllamaClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

OllamaClient::OllamaClient(QObject *parent)
    : QObject(parent),
      m_network(new QNetworkAccessManager(this)),
      m_timeoutTimer(new QTimer(this)),
      m_endpoint(QUrl(qEnvironmentVariable("AEROSIM_OLLAMA_ENDPOINT",
                                           QStringLiteral("http://127.0.0.1:11434")))),
      m_model(qEnvironmentVariable("AEROSIM_OLLAMA_MODEL",
                                   QStringLiteral("qwen2.5-coder:7b")))
{
    const int configuredContext =
        qEnvironmentVariableIntValue("AEROSIM_OLLAMA_NUM_CTX");
    if (configuredContext > 0)
        m_contextLength = configuredContext;

    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]()
            {
                if (!m_reply)
                    return;
                QNetworkReply *reply = m_reply;
                m_reply = nullptr;
                reply->abort();
                qWarning().noquote() << "[OLLAMA] timeout after" << m_timeoutMs << "ms";
                emit requestFailed(QStringLiteral("OLLAMA REQUEST TIMED OUT"), true);
                reply->deleteLater(); });
}

QUrl OllamaClient::endpoint() const { return m_endpoint; }
QString OllamaClient::model() const { return m_model; }
int OllamaClient::timeoutMs() const { return m_timeoutMs; }

void OllamaClient::setEndpoint(const QUrl &endpoint)
{
    if (endpoint == m_endpoint)
        return;
    m_endpoint = endpoint;
    emit endpointChanged();
}

void OllamaClient::setModel(const QString &model)
{
    if (model == m_model)
        return;
    m_model = model;
    emit modelChanged();
}

void OllamaClient::setTimeoutMs(int timeoutMs)
{
    if (timeoutMs < 1000 || timeoutMs == m_timeoutMs)
        return;
    m_timeoutMs = timeoutMs;
    emit timeoutMsChanged();
}

void OllamaClient::generate(const QString &prompt)
{
    qDebug().noquote() << "[OLLAMA] generate requested; pending=" << (m_reply != nullptr);
    if (m_reply)
    {
        emit requestFailed(QStringLiteral("OLLAMA REQUEST ALREADY IN PROGRESS"), false);
        return;
    }
    if (!m_endpoint.isValid() || m_model.trimmed().isEmpty())
    {
        emit requestFailed(QStringLiteral("OLLAMA CONFIGURATION IS INVALID"), false);
        return;
    }

    QUrl url = m_endpoint;
    QString path = url.path();
    if (!path.endsWith(QLatin1Char('/')))
        path += QLatin1Char('/');
    url.setPath(path + QStringLiteral("api/generate"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    const QJsonObject options{
        {QStringLiteral("num_ctx"), m_contextLength},
        {QStringLiteral("num_gpu"), 0}
    };
    const QJsonObject payload{{QStringLiteral("model"), m_model},
                              {QStringLiteral("prompt"), prompt},
                              {QStringLiteral("stream"), false},
                              {QStringLiteral("options"), options}};
    const QByteArray requestBody =
        QJsonDocument(payload).toJson(QJsonDocument::Compact);
    qInfo().noquote() << "[OLLAMA] HTTP request; method=POST"
                      << "url=" << url.toString(QUrl::FullyEncoded)
                      << "contentType=application/json"
                      << "body=" << QString::fromUtf8(requestBody);
    m_reply = m_network->post(request, requestBody);
    QNetworkReply *reply = m_reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                if (reply == m_reply)
                    finishReply(reply); });
    m_timeoutTimer->start(m_timeoutMs);
    qDebug().noquote() << "[OLLAMA] request started; timeoutMs=" << m_timeoutMs;
    emit requestStarted();
}

void OllamaClient::finishReply(QNetworkReply *reply)
{
    if (reply != m_reply)
    {
        qWarning().noquote() << "[OLLAMA] ignored stale reply callback";
        return;
    }
    m_timeoutTimer->stop();
    m_reply = nullptr;
    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    qDebug().noquote() << "[OLLAMA] HTTP response; status=" << httpStatus
                       << "networkError=" << reply->error()
                       << "errorString=" << reply->errorString()
                       << "body=" << QString::fromUtf8(body);
    const bool unavailable = reply->error() == QNetworkReply::ConnectionRefusedError ||
                             reply->error() == QNetworkReply::HostNotFoundError ||
                             reply->error() == QNetworkReply::TimeoutError;
    if (httpStatus >= 400)
    {
        qWarning().noquote() << "[OLLAMA] HTTP error response; status=" << httpStatus;
        const QJsonDocument errorDocument = QJsonDocument::fromJson(body);
        const QString serverError = errorDocument.isObject()
                                        ? errorDocument.object()
                                              .value(QStringLiteral("error"))
                                              .toString()
                                              .trimmed()
                                        : QString();
        const QString detail = serverError.isEmpty()
                                   ? QString()
                                   : QStringLiteral(": %1").arg(serverError);
        emit requestFailed(QStringLiteral("OLLAMA HTTP ERROR %1%2")
                               .arg(httpStatus)
                               .arg(detail),
                           false);
    }
    else if (reply->error() != QNetworkReply::NoError)
    {
        qWarning().noquote() << "[OLLAMA] network error; code=" << reply->error()
                             << "detail=" << reply->errorString();
        emit requestFailed(QStringLiteral("OLLAMA UNAVAILABLE: %1").arg(reply->errorString()),
                           unavailable);
    }
    else
    {
        const QJsonDocument document = QJsonDocument::fromJson(body);
        if (!document.isObject() ||
            !document.object().value(QStringLiteral("response")).isString())
        {
            qWarning().noquote() << "[OLLAMA] invalid response envelope JSON";
            emit requestFailed(QStringLiteral("OLLAMA RESPONSE IS INVALID JSON"), false);
        }
        else
        {
            const QString response =
                document.object().value(QStringLiteral("response")).toString();
            qDebug().noquote() << "[OLLAMA] model response:"
                               << response.left(500);
            emit responseReady(response);
        }
    }
    reply->deleteLater();
}
