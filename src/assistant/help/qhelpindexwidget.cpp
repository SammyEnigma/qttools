// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qhelpindexwidget.h"
#include "qhelpcollectionhandler_p.h"
#include "qhelpenginecore.h"
#include "qhelpfilterengine.h"
#include "qhelplink.h"

#include <QtCore/qthread.h>
#include <QtCore/qmutex.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

class QHelpIndexProvider final : public QThread
{
public:
    QHelpIndexProvider(QHelpEngineCore *helpEngine)
        : QThread(helpEngine)
        , m_helpEngine(helpEngine)
    {}

    ~QHelpIndexProvider() final
    {
        stopCollecting();
    }

    void collectIndicesForCurrentFilter();
    void collectIndices(const QString &customFilterName);
    void stopCollecting();
    QStringList indices() const;

private:
    void run() override;

    QHelpEngineCore *m_helpEngine;
    QString m_currentFilter;
    QStringList m_filterAttributes;
    QStringList m_indices;
    mutable QMutex m_mutex;
};

class QHelpIndexModelPrivate
{
public:
    QHelpIndexModelPrivate(QHelpEngineCore *helpEngine)
        : m_helpEngine(helpEngine)
        , indexProvider(new QHelpIndexProvider(helpEngine))
    {}

    QHelpEngineCore *m_helpEngine;
    QHelpIndexProvider *indexProvider;
    QStringList indices;
};

void QHelpIndexProvider::collectIndicesForCurrentFilter()
{
    m_mutex.lock();
    m_currentFilter = m_helpEngine->filterEngine()->activeFilter();
    m_filterAttributes = m_helpEngine->filterAttributes(m_helpEngine->legacyCurrentFilterName());
    m_mutex.unlock();

    if (isRunning())
        stopCollecting();
    start(LowPriority);
}

void QHelpIndexProvider::collectIndices(const QString &customFilterName)
{
    m_mutex.lock();
    m_currentFilter = customFilterName;
    m_filterAttributes = m_helpEngine->filterAttributes(customFilterName);
    m_mutex.unlock();

    if (isRunning())
        stopCollecting();
    start(LowPriority);
}

void QHelpIndexProvider::stopCollecting()
{
    if (!isRunning())
        return;
    wait();
}

QStringList QHelpIndexProvider::indices() const
{
    QMutexLocker lck(&m_mutex);
    return m_indices;
}

void QHelpIndexProvider::run()
{
    m_mutex.lock();
    const QString currentFilter = m_currentFilter;
    const QStringList attributes = m_filterAttributes;
    const QString collectionFile = m_helpEngine->collectionFile();
    m_indices = QStringList();
    m_mutex.unlock();

    if (collectionFile.isEmpty())
        return;

    QHelpCollectionHandler collectionHandler(collectionFile);
    if (!collectionHandler.openCollectionFile())
        return;

    const QStringList result = m_helpEngine->usesFilterEngine()
            ? collectionHandler.indicesForFilter(currentFilter)
            : collectionHandler.indicesForFilter(attributes);

    m_mutex.lock();
    m_indices = result;
    m_mutex.unlock();
}

/*!
    \class QHelpIndexModel
    \since 4.4
    \inmodule QtHelp
    \brief The QHelpIndexModel class provides a model that
    supplies index keywords to views.


*/

/*!
    \fn void QHelpIndexModel::indexCreationStarted()

    This signal is emitted when the creation of a new index
    has started. The current index is invalid from this
    point on until the signal indexCreated() is emitted.

    \sa isCreatingIndex()
*/

/*!
    \fn void QHelpIndexModel::indexCreated()

    This signal is emitted when the index has been created.
*/

QHelpIndexModel::QHelpIndexModel(QHelpEngineCore *helpEngine)
    : QStringListModel(helpEngine)
    , d(new QHelpIndexModelPrivate(helpEngine))
{
    connect(d->indexProvider, &QThread::finished, this, &QHelpIndexModel::insertIndices);
}

QHelpIndexModel::~QHelpIndexModel()
{
    delete d;
}

/*!
    \since 6.8

    Creates a new index by querying the help system for keywords for the current filter.
*/
void QHelpIndexModel::createIndexForCurrentFilter()
{
    const bool running = d->indexProvider->isRunning();
    d->indexProvider->collectIndicesForCurrentFilter();
    if (running)
        return;

    d->indices.clear();
    filter({});
    emit indexCreationStarted();
}

/*!
    Creates a new index by querying the help system for
    keywords for the specified \a customFilterName.
*/
void QHelpIndexModel::createIndex(const QString &customFilterName)
{
    const bool running = d->indexProvider->isRunning();
    d->indexProvider->collectIndices(customFilterName);
    if (running)
        return;

    d->indices.clear();
    filter({});
    emit indexCreationStarted();
}

void QHelpIndexModel::insertIndices()
{
    if (d->indexProvider->isRunning())
        return;

    d->indices = d->indexProvider->indices();
    filter({});
    emit indexCreated();
}

/*!
    Returns true if the index is currently built up, otherwise
    false.
*/
bool QHelpIndexModel::isCreatingIndex() const
{
    return d->indexProvider->isRunning();
}

/*!
    \since 5.15

    Returns the associated help engine that manages this model.
*/
QHelpEngineCore *QHelpIndexModel::helpEngine() const
{
    return d->m_helpEngine;
}

/*!
    Filters the indices and returns the model index of the best
    matching keyword. In a first step, only the keywords containing
    \a filter are kept in the model's index list. Analogously, if
    \a wildcard is not empty, only the keywords matched are left
    in the index list. In a second step, the best match is
    determined and its index model returned. When specifying a
    wildcard expression, the \a filter string is used to
    search for the best match.
*/
QModelIndex QHelpIndexModel::filter(const QString &filter, const QString &wildcard)
{
    if (filter.isEmpty()) {
        setStringList(d->indices);
        return index(-1, 0, {});
    }

    QStringList lst;
    int goodMatch = -1;
    int perfectMatch = -1;

    if (!wildcard.isEmpty()) {
        auto re = QRegularExpression::wildcardToRegularExpression(wildcard,
                  QRegularExpression::UnanchoredWildcardConversion);
        const QRegularExpression regExp(re, QRegularExpression::CaseInsensitiveOption);
        // TODO: Limit code repetition, see next loop in this body
        for (const QString &index : std::as_const(d->indices)) {
            if (index.contains(regExp)) {
                lst.append(index);
                if (perfectMatch == -1 && index.startsWith(filter, Qt::CaseInsensitive)) {
                    if (goodMatch == -1)
                        goodMatch = lst.size() - 1;
                    if (filter.size() == index.size())
                        perfectMatch = lst.size() - 1;
                } else if (perfectMatch > -1 && index == filter) {
                    perfectMatch = lst.size() - 1;
                }
            }
        }
    } else {
        for (const QString &index : std::as_const(d->indices)) {
            if (index.contains(filter, Qt::CaseInsensitive)) {
                lst.append(index);
                if (perfectMatch == -1 && index.startsWith(filter, Qt::CaseInsensitive)) {
                    if (goodMatch == -1)
                        goodMatch = lst.size() - 1;
                    if (filter.size() == index.size())
                        perfectMatch = lst.size() - 1;
                } else if (perfectMatch > -1 && index == filter) {
                    perfectMatch = lst.size() - 1;
                }
            }
        }
    }

    if (perfectMatch == -1)
        perfectMatch = qMax(0, goodMatch);

    setStringList(lst);
    return index(perfectMatch, 0, {});
}

/*!
    \class QHelpIndexWidget
    \inmodule QtHelp
    \since 4.4
    \brief The QHelpIndexWidget class provides a list view
    displaying the QHelpIndexModel.
*/

/*!
    \fn void QHelpIndexWidget::linkActivated(const QUrl &link,
        const QString &keyword)

    \deprecated

    Use documentActivated() instead.

    This signal is emitted when an item is activated and its
    associated \a link should be shown. To know where the link
    belongs to, the \a keyword is given as a second parameter.
*/

/*!
    \fn void QHelpIndexWidget::documentActivated(const QHelpLink &document,
        const QString &keyword)

    \since 5.15

    This signal is emitted when an item is activated and its
    associated \a document should be shown. To know where the link
    belongs to, the \a keyword is given as a second parameter.
*/

/*!
    \fn void QHelpIndexWidget::documentsActivated(const QList<QHelpLink> &documents,
        const QString &keyword)

    \since 5.15

    This signal is emitted when the item representing the \a keyword
    is activated and the item has more than one document associated.
    The \a documents consist of the document titles and their URLs.
*/

QHelpIndexWidget::QHelpIndexWidget()
{
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setUniformItemSizes(true);
    connect(this, &QAbstractItemView::activated, this, &QHelpIndexWidget::showLink);
}

void QHelpIndexWidget::showLink(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    QHelpIndexModel *indexModel = qobject_cast<QHelpIndexModel*>(model());
    if (!indexModel)
        return;

    const QVariant &v = indexModel->data(index, Qt::DisplayRole);
    const QString name = v.isValid() ? v.toString() : QString();

    const QList<QHelpLink> &docs = indexModel->helpEngine()->documentsForKeyword(name);
    if (docs.size() > 1) {
        emit documentsActivated(docs, name);
#if QT_DEPRECATED_SINCE(5, 15)
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        QMultiMap<QString, QUrl> links;
        for (const auto &doc : docs)
            links.insert(doc.title, doc.url);
        emit linksActivated(links, name);
        QT_WARNING_POP
#endif
    } else if (!docs.isEmpty()) {
        emit documentActivated(docs.first(), name);
#if QT_DEPRECATED_SINCE(5, 15)
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        emit linkActivated(docs.first().url, name);
        QT_WARNING_POP
#endif
    }
}

/*!
    Activates the current item which will result eventually in
    the emitting of a linkActivated() or linksActivated()
    signal.
*/
void QHelpIndexWidget::activateCurrentItem()
{
    showLink(currentIndex());
}

/*!
    Filters the indices according to \a filter or \a wildcard.
    The item with the best match is set as current item.

    \sa QHelpIndexModel::filter()
*/
void QHelpIndexWidget::filterIndices(const QString &filter, const QString &wildcard)
{
    QHelpIndexModel *indexModel = qobject_cast<QHelpIndexModel *>(model());
    if (!indexModel)
        return;
    const QModelIndex &idx = indexModel->filter(filter, wildcard);
    if (idx.isValid())
        setCurrentIndex(idx);
}

QT_END_NAMESPACE
