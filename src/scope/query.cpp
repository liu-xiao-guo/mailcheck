#include <boost/algorithm/string/trim.hpp>

#include <scope/localization.h>
#include <scope/query.h>

#include <unity/scopes/Annotation.h>
#include <unity/scopes/CategorisedResult.h>
#include <unity/scopes/CategoryRenderer.h>
#include <unity/scopes/QueryBase.h>
#include <unity/scopes/SearchReply.h>

#include <iomanip>
#include <sstream>

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>

namespace sc = unity::scopes;
namespace alg = boost::algorithm;

using namespace std;
using namespace api;
using namespace scope;
using namespace unity::scopes;

QString m_scopePath;
QString m_rootDepartmentId;
QMap<QString, std::string> m_renders;
QString m_userAgent;
QString m_imageDefault;
QString m_imageError;
QMap<QString, QString> m_depts;
QString m_curScopeId;

#define qstr(s) QString::fromStdString(s)

static QMap<QString, QString> DEPARTMENT_LAYOUTS {
};

#define FOREACH_JSON(it, results) QJsonArray::iterator it; \
    for (it = results.begin(); it != results.end(); ++it)

#define SET_RESULT(key, value) result[key] = value.toStdString()

#define LOAD_RENDERER(which) m_renders.insert(which, getRenderer(m_scopePath, which))

Query::Query(const sc::CannedQuery &query, const sc::SearchMetadata &metadata,
             QString scopePath, Config::Ptr config) :
    sc::SearchQueryBase(query, metadata), client_(config) {
    m_scopePath = scopePath;
    m_userAgent = QString("%1 (Ubuntu)").arg(SCOPE_PACKAGE);
    m_imageDefault = QString("file://%1/images/%2").arg(scopePath).arg(IMG_DEFAULT);
    m_imageError = QString("file://%1/images/%2").arg(scopePath).arg(IMG_ERROR);

    // You can comment out the renderers you don't use.
    LOAD_RENDERER("journal");
    LOAD_RENDERER("wide-art");
    LOAD_RENDERER("hgrid");
    LOAD_RENDERER("carousel");
    LOAD_RENDERER("large");
}

std::string Query::getRenderer(QString scopePath, QString name) {
    QString renderer = readFile(QString("%1/renderer/%2.json")
                                .arg(scopePath).arg(name));
    return renderer.toStdString();
}

void Query::cancelled() {
    client_.cancel();
}

void Query::run(sc::SearchReplyProxy const& reply) {
    try {
        // Start by getting information about the query
        const CannedQuery &query(sc::SearchQueryBase::query());

        // Trim the query string of whitespace
        string query_string = alg::trim_copy(query.query_string());
        QString queryString = QString::fromStdString(query_string);

        qDebug() << "queryString: " << queryString;

        // Only push the departments when the query string is null
        if (queryString.length() == 0) {
            qDebug() << "it is going to push the departments...!";
            pushDepartments(reply);
        }

        search(reply);

    } catch (domain_error &e) {
        // Handle exceptions being thrown by the client API
        cerr << e.what() << endl;
        reply->error(current_exception());
    }
}

void Query::search(sc::SearchReplyProxy const& reply) {
    CategoryRenderer renderer(m_renders.value("journal", ""));
    auto search = reply->register_category(
                "search", RESULTS.toStdString(), "", renderer);

    CannedQuery cannedQuery = SearchQueryBase::query();

    QString deptId = QString::fromStdString(cannedQuery.department_id());
    qDebug() << "deptId: " << deptId;

    qDebug() << "m_rootDepartmentId: " << m_rootDepartmentId;
    QString url;

    qDebug() << "m_curScopeId: " << m_curScopeId;

    if ( !deptId.isEmpty() ) {
        m_curScopeId = deptId;
    }

    if ( deptId.isEmpty() && !m_rootDepartmentId.isEmpty()
         && m_curScopeId == m_rootDepartmentId ) {
        qDebug() << "Going to get the surfacing content";
        url = QString(m_depts[m_rootDepartmentId]).arg(592833849048);
    } else {
        QString queryString = QString::fromStdString(cannedQuery.query_string());
        qDebug() << "queryString: " << queryString;

        qDebug() << "m_curScopeId1: " << m_curScopeId;
        qDebug() << "it comes here!";

        // Dump the departments. The map has been sorted out
        QMapIterator<QString, QString> i(m_depts);
        qDebug() << "m_depts count: "  << m_depts.count();

        while (i.hasNext()) {
            i.next();
            qDebug() << "scope id: " << i.key() << ": " << i.value();
        }

        url = m_depts[m_curScopeId].arg(queryString);
    }

    qDebug() << "url: "  << url;
    qDebug() << "m_curScopeId: " << m_curScopeId;

    try {
        QByteArray data = get(reply, QUrl(url));
        getMailInfo(data, reply);
    } catch (domain_error &e ) {
        cerr << e.what() << endl;
        reply->error(current_exception());
    }
}

void Query::getMailInfo(QByteArray &data, SearchReplyProxy const& reply) {
    // qDebug() << "data: " << data;

    QJsonParseError e;
    QJsonDocument document = QJsonDocument::fromJson(data, &e);
    if (e.error != QJsonParseError::NoError) {
        throw QString("Failed to parse response: %1").arg(e.errorString());
    }

    QJsonObject obj = document.object();

    //    QString message = obj.value("message").toString();
    //    qDebug() << "message: " << message;

    //    auto message1  = obj["message"].toString();
    //    qDebug() << "message1: " << message1;

    qDebug() << "***********************\r\n";

    if (obj.contains("data")) {
        qDebug() << "it has data!";

        QJsonValue data1 = obj.value("data");

        QJsonArray results = data1.toArray();

        CannedQuery query = SearchQueryBase::query();
        QString queryString = qstr(query.query_string());
        QString department = qstr(query.department_id());
        QString layout = DEPARTMENT_LAYOUTS.value(department);
        std::string renderTemplate;

        if (m_renders.contains(layout))
            renderTemplate = m_renders.value(layout, "");
        else
            renderTemplate = m_renders.value("journal");

        CategoryRenderer grid(renderTemplate);
        std::string categoryId = "root";
        std::string categoryTitle = " "; // #1330899 workaround
        std::string icon = "";
        auto tracking = reply->register_category(categoryId, categoryTitle, icon, grid);

        FOREACH_JSON(result, results) {
            QJsonObject o = (*result).toObject();

            QString time = o.value("time").toString();
//            qDebug() << "time: " << time;

            QString context = o.value("context").toString();
//            qDebug() << "context: " << context;

            QString link = "http://www.kuaidi100.com/";
            QString defaultImage ="file://"+ m_scopePath + "/images/" + m_curScopeId + ".jpg";

            CategorisedResult result(tracking);

            SET_RESULT("uri", link);
            SET_RESULT("image", defaultImage);
            //            SET_RESULT("video", video);
            SET_RESULT("title", time);
            //            SET_RESULT("subtitle", context);
            SET_RESULT("summary", context);
            //            SET_RESULT("full_summary", fullSummary);
            //            result["actions"] = actions.end();

            if (!reply->push(result)) break;
        }
    }
}

QByteArray Query::get(sc::SearchReplyProxy const& reply, QUrl url) const {
    QNetworkRequest request(url);
    QByteArray data = makeRequest(reply, request);
    return data;
}

void Query::pushDepartments(unity::scopes::SearchReplyProxy const& reply) {
    qDebug() << "mScopePath: "  << m_scopePath;

    QString json = getDepartmentsData( m_scopePath );

    QJsonDocument doc = QJsonDocument::fromJson( json.toUtf8() );
    if (doc.isNull()) {
        qCritical() << "Failed to parse departments JSON!";
        return;
    }

    CannedQuery query = SearchQueryBase::query();
    QString queryString = QString::fromStdString(query.query_string());

    DepartmentList depts = getDepartments( doc );
    Department::SPtr root = getRootDepartment( depts );
    root->set_subdepartments(depts);

    reply->register_departments(root);
}

Department::SPtr Query::getRootDepartment(DepartmentList &depts) {
    //Department::SPtr top = depts.front();
    std::shared_ptr<const Department> top = depts.front();
    std::string label;

    if (BROWSE_AT_ROOT) {
        label = BROWSE.toStdString();
    } else {
        label = top->label();
        depts.pop_front();
    }
    std::string id = top->id();

    // Idea: We could show last browsed department instead.
    m_curScopeId =QString::fromStdString(id);
    qDebug() << "Init m_curScopeId: " << m_curScopeId;

    m_rootDepartmentId = QString::fromStdString(id);
    qDebug() <<  "Init m_rootDepartmentId: " << m_rootDepartmentId;

    CannedQuery topQuery(SCOPENAME.toStdString());
    topQuery.set_department_id(id);
    Department::SPtr root(Department::create("", topQuery, label));
    return root;
}

QString Query::getDepartmentsData(QString scopePath) {
    QString path = QString("%1/departments.json").arg(scopePath);
    qDebug() << "Departments file path: " << path;

    return readFile(path);
}

QString Query::readFile(QString path) {
    QFile file(path);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QString data = file.readAll();
    // qDebug() << "JSON file: " << data;
    file.close();
    return data;
}

#define FOREACH_JSON(it, results) QJsonArray::iterator it; \
    for (it = results.begin(); it != results.end(); ++it)

DepartmentList Query::getDepartments(QJsonArray data) {
    qDebug() << "entering getDepartments";

    DepartmentList depts;

    // Clear the previous departments since the URL may change according to settings
    m_depts.clear();
    qDebug() << "m_depts is being cleared....!";

    int index = 0;
    FOREACH_JSON(json, data) {
        auto feed = (*json).toObject();
        QString title = feed["title"].toString();
        //        qDebug() << "title: " << title;

        QString url = feed["url"].toString();
        //        qDebug() << "url: " << url;

        QString pinyin = feed["pinyin"].toString();
        qDebug() << "pinyin: " << pinyin;

        QString layout = SURFACING_LAYOUT;

        if (feed.contains("layout")) {
            layout = feed["layout"].toString();
        }

        // save the depart
        m_depts.insert( pinyin, url );

        DEPARTMENT_LAYOUTS.insert(url, layout);

        // Dump the deparmemts
        //        QMapIterator<QString, QString> i(DEPARTMENT_LAYOUTS);
        //        while (i.hasNext()) {
        //            i.next();
        //             qDebug() << "key: " << i.key() << ": " << i.value();
        //        }

        CannedQuery query(SCOPENAME.toStdString());
        query.set_department_id(url.toStdString());
        query.set_query_string(url.toStdString());

//        Department::SPtr dept(Department::create(
//                                  url.toStdString(), query, title.toStdString()));
        Department::SPtr dept(Department::create(
                                  pinyin.toStdString(), query, title.toStdString()));

        //        if (feed.contains("departments")) {
        //            qDebug() << "feed contains departments";
        //            QJsonArray subdepartments = feed["departments"].toArray();
        //            DepartmentList subdepts = getDepartments(subdepartments);
        //            dept->set_subdepartments(subdepts);
        //        }

        depts.push_back(dept);

        index++;
    }

    // Dump the departments. The map has been sorted out
    QMapIterator<QString, QString> i(m_depts);
    while (i.hasNext()) {
        i.next();
        qDebug() << "scope id: " << i.key() << ": " << i.value();
    }

    return depts;
}

DepartmentList Query::getDepartments(QJsonDocument document) {
    DEPARTMENT_LAYOUTS.insert("", SURFACING_LAYOUT);
    QJsonArray jsonDepts = document.array();
    return getDepartments(jsonDepts);
}

QByteArray Query::makeRequest(SearchReplyProxy const& reply,QNetworkRequest &request) const {
    int argc = 1;
    char *argv = const_cast<char*>("rss-scope");
    // QCoreApplication *app = new QCoreApplication(argc, &argv);
    QEventLoop loop;

    QNetworkAccessManager manager;
    QByteArray response;
    QNetworkDiskCache *cache = new QNetworkDiskCache();
    QString cachePath = m_scopePath + "/cache";
    //qDebug() << "Cache dir: " << cachePath;
    cache->setCacheDirectory(cachePath);

    request.setRawHeader("User-Agent", m_userAgent.toStdString().c_str());
    request.setRawHeader("Content-Type", "application/rss+xml, text/xml");
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);

//    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), app, SLOT(quit()));
    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &loop, SLOT(quit()));
    QObject::connect(&manager, &QNetworkAccessManager::finished,
                     [this, &reply, &response](QNetworkReply *msg) {
        if (msg->error() != QNetworkReply::NoError) {
            qCritical() << "Failed to get data: " << msg->error();
            // pushError(reply, NO_CONNECTION);
        } else {
            response = msg->readAll();
        }
        msg->deleteLater();
    });

    manager.setCache(cache);
    manager.get(request);
    // app->exec();
    loop.exec();

    delete cache;
    return response;
}

void Query::pushError(SearchReplyProxy const& reply, QString error) {
    CategoryRenderer renderer(m_renders.value("hgrid", ""));
    auto errors = reply->register_category("error", "", "", renderer);

    CategorisedResult result(errors);
    result["uri"] = HOME_URL.toStdString();
    result["title"] = ERROR.toStdString();
    result["subtitle"] = error.toStdString();
    result["image"] = m_imageError.toStdString();
    reply->push(result);
}
