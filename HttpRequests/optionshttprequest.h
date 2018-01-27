/*
 *
Copyright (C) 2017-2018  Fábio Bento (fabiobento512)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#ifndef OPTIONSHTTPREQUEST_H
#define OPTIONSHTTPREQUEST_H

#include "httprequestwithmultipart.h"

class OptionsHttpRequest : public HttpRequestWithMultiPart
{
public:
    OptionsHttpRequest(
            QNetworkAccessManager * const manager,
            QTableWidget * const twBodyFormKeyValue,
            const QString &fullPath,
            const QString &bodyType,
            const QString &rawRequestBody,
            const QVector<UtilFRequest::HttpHeader> &requestHeaders
            );
private:
    QNetworkReply* sendRequest(const QNetworkRequest &request, const QByteArray &data);
	QNetworkReply* sendRequest(const QNetworkRequest &request, QHttpMultiPart &data);
};

#endif // OPTIONSHTTPREQUEST_H
