#include "httprequest.h"

HttpRequest::HttpRequest
(
        QNetworkAccessManager * const manager,
        QTableWidget * const twBodyFormKeyValue,
        const QString &fullPath,
        const QString &bodyType,
        const QString &rawRequestBody,
        const QVector<UtilFRequest::HttpHeader> &requestHeaders
        )
    :fullPath(fullPath),
      requestHeaders(requestHeaders), bodyType(bodyType),
      rawRequestBody(rawRequestBody), manager(manager), twBodyFormKeyValue(twBodyFormKeyValue)
{

}

HttpRequest::HttpRequest
(
        QNetworkAccessManager * const manager,
        const QString &fullPath,
        const QVector<UtilFRequest::HttpHeader> &requestHeaders
        )
    :fullPath(fullPath),
      requestHeaders(requestHeaders),
      manager(manager),
      twBodyFormKeyValue(nullptr)
{

}

HttpRequest::~HttpRequest(){

}

QNetworkReply* HttpRequest::processRequest(){

    QNetworkRequest request(QUrl(this->fullPath));

    for(const UtilFRequest::HttpHeader &currentHeader : this->requestHeaders){
        request.setRawHeader(currentHeader.name.toUtf8(), currentHeader.value.toUtf8());
    }

    if(!bodyType.isEmpty()){
        if(this->bodyType == "raw"){
            return sendRequest(request, this->rawRequestBody.toUtf8());
        }
        else if(this->bodyType == "form-data"){
            return sendFormRequest(request);
        }
        else if(this->bodyType == "x-form-www-urlencoded"){
            return sendFormRequest(request);
        }
        else{
            QString errorMessage = "Body type unknown: '" + this->bodyType + "'. Application can't proceed.";
            Util::Dialogs::showError(errorMessage);
            LOG_FATAL << errorMessage;
            exit(1);
        }
    }
    else{ // Get, delete etc which doesn't have a "body"
        return sendRequest(request, QString().toUtf8());
    }
}

QNetworkReply* HttpRequest::sendFormRequest(QNetworkRequest &request){
    QUrlQuery params;

    for(int i = 0; i < this->twBodyFormKeyValue->rowCount(); i++){
        params.addQueryItem(this->twBodyFormKeyValue->item(i,0)->text(), this->twBodyFormKeyValue->item(i,1)->text());
    }

    return sendRequest(request, params.toString(QUrl::FullyEncoded).toUtf8());
}

QNetworkReply* HttpRequest::sendHttpCustomRequest(const QNetworkRequest &request, const QString &verb, const QByteArray &data){

    // Based from here:
    // https://stackoverflow.com/a/34065736/1499019
    QBuffer *buffer=new QBuffer();
    if(!data.isNull() && !data.isEmpty()){
        buffer->open((QBuffer::ReadWrite));
        buffer->write(data);
        buffer->seek(0);
    }

    return this->manager->sendCustomRequest(request, QSTR_TO_CSTR(verb), buffer);
}