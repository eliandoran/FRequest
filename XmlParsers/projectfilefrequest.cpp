/*
 *
Copyright (C) 2017-2019  Fábio Bento (fabiobento512)

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

#include "projectfilefrequest.h"

ProjectFileFRequest::ProjectData ProjectFileFRequest::readProjectDataFromFile(const QString &fileFullPath){

    ProjectFileFRequest::ProjectData currentProjectData;

    upgradeProjectFileIfNecessary(fileFullPath);

    pugi::xml_document doc;

    pugi::xml_parse_result result = doc.load_file(QSTR_TO_CSTR(fileFullPath));

    if(result.status!=pugi::status_ok){
        throw std::runtime_error(QSTR_TO_CSTR(QString("An error ocurred while loading project file.\n") + result.description()));
    }

    if(QString(doc.root().first_child().name()) != "FRequestProject"){
        throw std::runtime_error(QSTR_TO_CSTR(QString(doc.root().name()) + "The file opened is not a valid FRequestProject file. Load aborted."));
    }

    QString projFRequestVersion;

    try{
        projFRequestVersion = QString(doc.select_node("/FRequestProject/@frequestVersion").attribute().value());
    }
    catch (const pugi::xpath_exception& e)
    {
        throw std::runtime_error(QSTR_TO_CSTR(QString("Couldn't find the frequestVersion of the current project. Load aborted.\n") + e.what()));
    }

    if(projFRequestVersion != GlobalVars::LastCompatibleVersionProjects){
        throw std::runtime_error("The project that you are trying to load seems it is not compatible with your FRequest Version. Please update FRequest and try again.");
    }

    // After the initial validations begin loading the project data

    currentProjectData.mainUrl = doc.select_node("/FRequestProject/@mainUrl").attribute().value();
    currentProjectData.projectName = doc.select_node("/FRequestProject/@name").attribute().value();
    currentProjectData.projectUuid = doc.select_node("/FRequestProject/@uuid").attribute().value();
    currentProjectData.saveIdentCharacter = static_cast<UtilFRequest::IdentCharacter>(doc.select_node("/FRequestProject/@saveIdentCharacter").attribute().as_int());

    // Check if there is authentication data and load it
    pugi::xml_node authNode = doc.select_node("/FRequestProject/Authentication").node();

    if(!authNode.empty()){
        FRequestAuthentication::AuthenticationType authType =
                static_cast<FRequestAuthentication::AuthenticationType>(authNode.attribute("type").as_int());

        const bool retryLoginIfError401 = authNode.attribute("bRetryLoginIfError").as_bool();

        switch(authType){
        case FRequestAuthentication::AuthenticationType::REQUEST_AUTHENTICATION:
        {
            QString username = authNode.child("Username").child_value();
            QString passwordSalt = authNode.child("PasswordSalt").child_value();
            QString password = QString(UtilFRequest::simpleStringObfuscationDeobfuscation(passwordSalt,
                                                                                          QByteArray::fromBase64(authNode.child("Password").child_value())));
            QString requestUuid = authNode.attribute("requestUuid").value();

            currentProjectData.authData = std::make_shared<RequestAuthentication>(RequestAuthentication(false, retryLoginIfError401, username, passwordSalt, password, requestUuid));

            break;
        }
        case FRequestAuthentication::AuthenticationType::BASIC_AUTHENTICATION:
        {
            QString username = authNode.child("Username").child_value();
            QString passwordSalt = authNode.child("PasswordSalt").child_value();
            QString password = QString(UtilFRequest::simpleStringObfuscationDeobfuscation(passwordSalt,
                                                                                          QByteArray::fromBase64(authNode.child("Password").child_value())));

            currentProjectData.authData = std::make_shared<BasicAuthentication>(BasicAuthentication(false, retryLoginIfError401, username, passwordSalt, password));
            break;
        }
        default:
        {
            QString errorMessage = "Authentication type unknown: '" + QString::number(static_cast<int>(authType)) + "'. Program can't proceed.";
            Util::Dialogs::showError(errorMessage);
            LOG_FATAL << errorMessage;
            exit(1);
        }
        }
    }

    // Fetch request items
    pugi::xpath_node_set requestNodes = doc.select_nodes("/FRequestProject/Request");

    // Aux lambda to avoid duplicated code for FORM_DATA / X_FORM_WWW_URLENCODED
    auto fGetBodyForm = [](pugi::xml_node &bodyNode) -> QVector<UtilFRequest::HttpFormKeyValueType>
    {
        QVector<UtilFRequest::HttpFormKeyValueType> bodyForm;

        for(const pugi::xml_node &currentFormKeyValueNode : bodyNode.children()){
            UtilFRequest::HttpFormKeyValueType currentFormKeyValue
                    (
                        currentFormKeyValueNode.child("Key").child_value(),
                        currentFormKeyValueNode.child("Value").child_value(),
                        static_cast<UtilFRequest::FormKeyValueType>(QString(currentFormKeyValueNode.child("Type").child_value()).toInt()) // TODO check if pugixml has any method do retreive int directly for elements / pcdata
                        );

            bodyForm.append(currentFormKeyValue);
        }

        return bodyForm;
    };

    for(size_t i=0; i < requestNodes.size(); i++){

        pugi::xml_node currNode = requestNodes[i].node();

        UtilFRequest::RequestInfo currentRequestInfo;

        currentRequestInfo.path = currNode.attribute("path").as_string();
        currentRequestInfo.requestType = static_cast<UtilFRequest::RequestType>(currNode.attribute("type").as_int());

        pugi::xml_node bodyNode = currNode.child("Body");

        currentRequestInfo.bodyType = static_cast<UtilFRequest::BodyType>(bodyNode.attribute("type").as_int());

        switch (currentRequestInfo.bodyType) {
        case UtilFRequest::BodyType::RAW:
        {
            currentRequestInfo.body = bodyNode.child_value();
            break;
        }
        case UtilFRequest::BodyType::FORM_DATA:
        {
            currentRequestInfo.bodyForm = fGetBodyForm(bodyNode);
            break;
        }
        case UtilFRequest::BodyType::X_FORM_WWW_URLENCODED:
        {
            currentRequestInfo.bodyForm = fGetBodyForm(bodyNode);
            break;
        }
        default:
        {
            break;
        }
        }

        QVector<UtilFRequest::HttpHeader> requestHeaders;

        for(const pugi::xml_node &currentHeaderNode : currNode.child("Headers").children()){
            UtilFRequest::HttpHeader currentRequestHeader;

            currentRequestHeader.name = QString(currentHeaderNode.child("Key").child_value());
            currentRequestHeader.value = currentHeaderNode.child("Value").child_value();

            requestHeaders.append(currentRequestHeader);
        }

        currentRequestInfo.headers = requestHeaders;

        currentRequestInfo.bOverridesMainUrl = currNode.attribute("bOverridesMainUrl").as_bool();
        currentRequestInfo.overrideMainUrl = currNode.attribute("overrideMainUrl").value();
        currentRequestInfo.bDownloadResponseAsFile = currNode.attribute("bDownloadResponseAsFile").as_bool();
        currentRequestInfo.name = requestNodes[i].node().attribute("name").value();
        currentRequestInfo.uuid = requestNodes[i].node().attribute("uuid").value();
        currentRequestInfo.order = requestNodes[i].node().attribute("order").as_ullong();
        currentRequestInfo.bDisableGlobalHeaders = currNode.attribute("bDisableGlobalHeaders").as_bool();

        currentProjectData.projectRequests.append(currentRequestInfo);
    }

    pugi::xml_node globalHeadersNode = doc.select_node("/FRequestProject/GlobalHeaders").node();

    QVector<UtilFRequest::HttpHeader> globalHeaders;

    for(const pugi::xml_node &currentGlobalHeaderNode : globalHeadersNode.children()){
        UtilFRequest::HttpHeader currentGlobalHeader;

        currentGlobalHeader.name = QString(currentGlobalHeaderNode.child("Key").child_value());
        currentGlobalHeader.value = currentGlobalHeaderNode.child("Value").child_value();

        globalHeaders.append(currentGlobalHeader);
    }
    currentProjectData.globalHeaders = globalHeaders;

    return currentProjectData;
}

void ProjectFileFRequest::saveProjectDataToFile(const QString &fileFullPath, const ProjectFileFRequest::ProjectData &newProjectData, const QVector<QString> &uuidsToCleanUp){
    pugi::xml_document doc;
    pugi::xml_node rootNode;

    bool isNewFile = !QFileInfo(fileFullPath).exists();

    // If file already exists try to read the project file
    if(!isNewFile){

        pugi::xml_parse_result result = doc.load_file(QSTR_TO_CSTR(fileFullPath));

        if(result.status!=pugi::status_ok){
            throw std::runtime_error(QSTR_TO_CSTR(QString("An error ocurred while loading project file.\n") + result.description()));
        }

        // Try to clear deleted items from projects file if they exist
        for(const QString &currentDeletedItemUuid : uuidsToCleanUp){
            pugi::xml_node nodeToDelete = doc.select_node(QSTR_TO_CSTR("/FRequestProject/Request[@uuid='" + currentDeletedItemUuid + "']")).node();

            if(!nodeToDelete.empty()){
                nodeToDelete.parent().remove_child(nodeToDelete);
            }
        }

        rootNode = doc.child("FRequestProject");
    }
    else{
        rootNode = doc.append_child("FRequestProject"); // create
    }

    createOrGetPugiXmlAttribute(rootNode, "frequestVersion").set_value(QSTR_TO_CSTR(GlobalVars::LastCompatibleVersionProjects));
    createOrGetPugiXmlAttribute(rootNode, "name").set_value(QSTR_TO_CSTR(newProjectData.projectName));
    createOrGetPugiXmlAttribute(rootNode, "mainUrl").set_value(QSTR_TO_CSTR(newProjectData.mainUrl));
    createOrGetPugiXmlAttribute(rootNode, "uuid").set_value(QSTR_TO_CSTR(newProjectData.projectUuid));
    createOrGetPugiXmlAttribute(rootNode, "saveIdentCharacter").set_value(static_cast<int>(newProjectData.saveIdentCharacter));

    // Delete old auth data if exists (we always need to rebuild it
    rootNode.remove_child("Authentication");

    // Save authentication data if it exists
    if(newProjectData.authData != nullptr && !newProjectData.authData->saveAuthToConfigFile){
        // please add it as the first node, since it is less likely to be removed than some request
        pugi::xml_node currentAuth = rootNode.prepend_child("Authentication");
        currentAuth.append_attribute("type").set_value(static_cast<int>(newProjectData.authData->type));
        currentAuth.append_attribute("bRetryLoginIfError").set_value(newProjectData.authData->retryLoginIfError401);

        switch(newProjectData.authData->type){
        case FRequestAuthentication::AuthenticationType::REQUEST_AUTHENTICATION:
        {
            const RequestAuthentication &requestAuth = static_cast<RequestAuthentication&>(*newProjectData.authData.get());

            currentAuth.append_attribute("requestUuid").set_value(QSTR_TO_CSTR(requestAuth.requestForAuthenticationUuid));
            currentAuth.append_child("Username").append_child(pugi::xml_node_type::node_pcdata).set_value(QSTR_TO_CSTR(requestAuth.username));
            currentAuth.append_child("PasswordSalt").append_child(pugi::xml_node_type::node_pcdata).set_value(QSTR_TO_CSTR(requestAuth.passwordSalt));
            currentAuth.append_child("Password").append_child(pugi::xml_node_type::node_pcdata).
                    set_value((QSTR_TO_CSTR(QString(UtilFRequest::simpleStringObfuscationDeobfuscation(requestAuth.passwordSalt, requestAuth.password).toBase64()))));
            break;
        }
        case FRequestAuthentication::AuthenticationType::BASIC_AUTHENTICATION:
        {
            const BasicAuthentication &basicAuth = static_cast<BasicAuthentication&>(*newProjectData.authData.get());
            currentAuth.append_child("Username").append_child(pugi::xml_node_type::node_pcdata).set_value(QSTR_TO_CSTR(basicAuth.username));
            currentAuth.append_child("PasswordSalt").append_child(pugi::xml_node_type::node_pcdata).set_value(QSTR_TO_CSTR(basicAuth.passwordSalt));
            currentAuth.append_child("Password").append_child(pugi::xml_node_type::node_pcdata).
                    set_value(QSTR_TO_CSTR(QString(UtilFRequest::simpleStringObfuscationDeobfuscation(basicAuth.passwordSalt, basicAuth.password).toBase64())));

            break;
        }
        default:
        {
            QString errorMessage = "Authentication type unknown: '" + QString::number(static_cast<int>(newProjectData.authData->type)) + "'. Program can't proceed.";
            Util::Dialogs::showError(errorMessage);
            LOG_FATAL << errorMessage;
            exit(1);
        }
        }
    }

    // Save by the current tree order
    for(const UtilFRequest::RequestInfo &currentRequest : newProjectData.projectRequests){

        pugi::xml_node requestNode;

        pugi::xpath_node xpathRequestNode = doc.select_node(QSTR_TO_CSTR("/FRequestProject/Request[@uuid='" + currentRequest.uuid + "']"));

        // if it doesn't exist yet create it
        if(xpathRequestNode.node().empty()){
            requestNode = rootNode.append_child("Request");
        }
        else{ // otherwise use the existing one
            requestNode = xpathRequestNode.node();
        }

        createOrGetPugiXmlAttribute(requestNode, "name").set_value(QSTR_TO_CSTR(currentRequest.name));
        createOrGetPugiXmlAttribute(requestNode, "uuid").set_value(QSTR_TO_CSTR(currentRequest.uuid));
        createOrGetPugiXmlAttribute(requestNode, "order").set_value(currentRequest.order);
        createOrGetPugiXmlAttribute(requestNode, "bOverridesMainUrl").set_value(currentRequest.bOverridesMainUrl);
        createOrGetPugiXmlAttribute(requestNode, "overrideMainUrl").set_value(QSTR_TO_CSTR(currentRequest.overrideMainUrl));
        createOrGetPugiXmlAttribute(requestNode, "path").set_value(QSTR_TO_CSTR(currentRequest.path));
        createOrGetPugiXmlAttribute(requestNode, "type").set_value(static_cast<int>(currentRequest.requestType));
        createOrGetPugiXmlAttribute(requestNode, "bDownloadResponseAsFile").set_value(currentRequest.bDownloadResponseAsFile);
        createOrGetPugiXmlAttribute(requestNode, "bDisableGlobalHeaders").set_value(currentRequest.bDisableGlobalHeaders);

        // remove body if exists, we will rebuild it
        requestNode.remove_child("Body");

        pugi::xml_node bodyNode = requestNode.append_child("Body");

        bodyNode.append_attribute("type").set_value(static_cast<int>(currentRequest.bodyType));

        // Aux lambda to avoid duplicated code for FORM_DATA / X_FORM_WWW_URLENCODED
        auto fSaveFormKeyValues = [](const QVector<UtilFRequest::HttpFormKeyValueType> &bodyForm, pugi::xml_node &bodyNode){
            for(const UtilFRequest::HttpFormKeyValueType &currentKeyValue : bodyForm){
                pugi::xml_node currentFormKeyValueNode = bodyNode.append_child("FormKeyValue");

                currentFormKeyValueNode.append_child("Key").append_child(pugi::xml_node_type::node_cdata).set_value(QSTR_TO_CSTR(currentKeyValue.key));
                currentFormKeyValueNode.append_child("Value").append_child(pugi::xml_node_type::node_cdata).set_value(QSTR_TO_CSTR(currentKeyValue.value));
                currentFormKeyValueNode.append_child("Type").append_child(pugi::node_pcdata).set_value(QSTR_TO_CSTR(QString::number(static_cast<int>(currentKeyValue.type)))); // TODO check if pugixml accepts int directly
            }
        };

        switch (currentRequest.bodyType) {
        case UtilFRequest::BodyType::RAW:
        {
            bodyNode.append_child(pugi::xml_node_type::node_cdata).set_value(QSTR_TO_CSTR(currentRequest.body));
            break;
        }
        case UtilFRequest::BodyType::FORM_DATA:
        {
            fSaveFormKeyValues(currentRequest.bodyForm, bodyNode);
            break;
        }
        case UtilFRequest::BodyType::X_FORM_WWW_URLENCODED:
        {
            fSaveFormKeyValues(currentRequest.bodyForm, bodyNode);
            break;
        }
        default:
        {
            break;
        }
        }

        // remove headers if they exist, we will rebuild them
        requestNode.remove_child("Headers");

        pugi::xml_node requestHeadersNode = requestNode.append_child("Headers");

        for(const UtilFRequest::HttpHeader &currentRequestHeader : currentRequest.headers){
            pugi::xml_node currentRequestHeaderNode = requestHeadersNode.append_child("Header");

            currentRequestHeaderNode.append_child("Key").append_child(pugi::xml_node_type::node_cdata).set_value(QSTR_TO_CSTR(currentRequestHeader.name));
            currentRequestHeaderNode.append_child("Value").append_child(pugi::xml_node_type::node_cdata).set_value(QSTR_TO_CSTR(currentRequestHeader.value));
        }

    }

    rootNode.remove_child("GlobalHeaders");

    pugi::xml_node globalHeadersNode = rootNode.append_child("GlobalHeaders");
    for (const UtilFRequest::HttpHeader &currentGlobalHeader: newProjectData.globalHeaders) {
        pugi::xml_node currentGlobalHeaderNode = globalHeadersNode.append_child("Header");

        currentGlobalHeaderNode.append_child("Key").append_child(pugi::xml_node_type::node_cdata).set_value(QSTR_TO_CSTR(currentGlobalHeader.name));
        currentGlobalHeaderNode.append_child("Value").append_child(pugi::xml_node_type::node_cdata).set_value(QSTR_TO_CSTR(currentGlobalHeader.value));
    }

    const pugi::char_t* const identCharacterChar = newProjectData.saveIdentCharacter == UtilFRequest::IdentCharacter::SPACE ? pugiIdentChars::spaceChar : pugiIdentChars::tabChar;

    if(!doc.save_file(fileFullPath.toUtf8().constData(), identCharacterChar, pugi::format_default | pugi::format_write_bom, pugi::xml_encoding::encoding_utf8)){
        throw std::runtime_error("An error ocurred while trying to save the project file. Please try another path.");
    }
}

pugi::xml_attribute ProjectFileFRequest::createOrGetPugiXmlAttribute(pugi::xml_node &mainNode, const char *name){
    // if attributes doesnt exists create it
    if(mainNode.attribute(name).empty())
    {
        return mainNode.append_attribute(name);
    }
    else
    { // if it already exists return it
        return mainNode.attribute(name);
    }
}

void ProjectFileFRequest::upgradeProjectFileIfNecessary(const QString &filePath){

    pugi::xml_document doc;

    pugi::xml_parse_result result = doc.load_file(QSTR_TO_CSTR(filePath));

    if(result.status!=pugi::status_ok){
        throw std::runtime_error(QSTR_TO_CSTR(QString("An error ocurred while loading project file.\n") + result.description()));
    }

    QString currProjectVersion = QString(doc.select_single_node("/FRequestProject").node().attribute("frequestVersion").as_string());

    if(currProjectVersion != GlobalVars::LastCompatibleVersionProjects){
        if(!Util::FileSystem::backupFile(filePath, filePath + UtilFRequest::getDateTimeFormatForFilename(QDateTime::currentDateTime()))){
            QString errorMessage = "Couldn't backup the existing project file for version upgrade, program can't proceed.";
            Util::Dialogs::showError(errorMessage);
            LOG_FATAL << errorMessage;
            exit(1);
        }
    }

    auto fUpgradeFileIfNecessary = [&doc, &filePath, &currProjectVersion](
            const QString &oldVersion,
            const QString &newVersion,
            const pugi::char_t* const identChar,
            std::function<void()> upgradeFunction){

        // Upgrade necessary?
        if(currProjectVersion == oldVersion){

            pugi::xml_node projectNode = doc.select_single_node("/FRequestProject").node();

            // Update version
            projectNode.attribute("frequestVersion").set_value(QSTR_TO_CSTR(newVersion));

            // do specific upgrade changes
            upgradeFunction();

            if(!doc.save_file(QSTR_TO_CSTR(filePath), identChar, pugi::format_default | pugi::format_write_bom, pugi::xml_encoding::encoding_utf8)){
                throw std::runtime_error(QSTR_TO_CSTR("Error while saving: '" + filePath + "'. After file version upgrade. (to version " + newVersion + " )"));
            }

            currProjectVersion = newVersion;
        }

    };

    fUpgradeFileIfNecessary("1.0", "1.1", pugiIdentChars::tabChar, [&](){

        pugi::xml_node projectNode = doc.select_single_node("/FRequestProject").node();

        // Generate an uuid to the project
        projectNode.append_attribute("uuid").set_value(QSTR_TO_CSTR(QUuid::createUuid().toString()));

        // Add types to form rows

        // Get form request bodies nodes to update
        pugi::xpath_node_set formKeyValuesNodes = doc.select_nodes("/FRequestProject/Request/Body/FormKeyValue");

        for(size_t i=0; i < formKeyValuesNodes.size(); i++){
            formKeyValuesNodes[i].node().append_child("Type").append_child(pugi::node_pcdata).set_value("0"); // 0 is Text in 1.1
        }

    });

    fUpgradeFileIfNecessary("1.1", "1.1c", pugiIdentChars::tabChar, [&](){

        pugi::xml_node projectNode = doc.select_single_node("/FRequestProject").node();

        // Set the default ident character
        // Previous to 1.1c all versions use tab as separator, so we want to keep that
        // until user changes, even though now space is the default
        projectNode.append_attribute("saveIdentCharacter").set_value("1"); // 0 is tab in 1.1c

    });

    const pugi::char_t* const currSaveIdentCharacter =
    pugiIdentChars::getIdentCharaterForEnum(static_cast<UtilFRequest::IdentCharacter>(
                                                doc.select_node("/FRequestProject/@saveIdentCharacter").attribute().as_int()));

    fUpgradeFileIfNecessary("1.1c", "1.2", currSaveIdentCharacter, [&](){

        pugi::xml_node projectNode = doc.select_single_node("/FrequestProject").node();

        projectNode.append_child("GlobalHeaders");

    });

    if(currProjectVersion != GlobalVars::LastCompatibleVersionProjects){
        throw std::runtime_error("Can't load the project file, it is from an incompatible version. Probably newer?");
    }
}

namespace pugiIdentChars {

const pugi::char_t* getIdentCharaterForEnum(const UtilFRequest::IdentCharacter identEnum){
    switch(identEnum){
    case UtilFRequest::IdentCharacter::SPACE:
    {
        return pugiIdentChars::spaceChar;
    }
    case UtilFRequest::IdentCharacter::TAB:
    {
        return pugiIdentChars::tabChar;
    }
    default:
    {
        QString errorMessage = "UtilFRequest::IdentCharacter type unknown: '" + QString::number(static_cast<int>(identEnum)) + "'. Program can't proceed.";
        Util::Dialogs::showError(errorMessage);
        LOG_FATAL << errorMessage;
        exit(1);
    }
    }
}

}
