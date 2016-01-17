/* QSyncable Project
   Author: Ben Lau
   License: Apache-2.0
   Web: https://github.com/benlau/qsyncable
*/
#include <QHash>
#include <QDebug>
#include "qsdiffrunner.h"

static QList<QSPatch> merge(const QList<QSPatch> list) {

    if (list.size() <= 1) {
        return list;
    }

    QList<QSPatch> res;

    QSPatch prev;

    for (int i = 0 ; i < list.size() ; i++) {
        QSPatch current = list.at(i);

        if (prev.isNull()) {
            prev = current;
            continue;
        }

        if (prev.canMerge(current)) {
            prev = prev.merge(current);
        } else {
            res << prev;
            prev = current;
        }
    }

    if (!prev.isNull()) {
        res << prev;
    }

    return res;
}

static QVariantMap compareMap(const QVariantMap& prev, const QVariantMap &current) {
    // To make this function faster, it won't track removed fields from prev.
    // Clear a field to null value should set it explicitly.

    QVariantMap diff;

    QMap<QString, QVariant>::const_iterator iter = current.begin();

    while (iter != current.end()) {
        QString key = iter.key();
        if (!prev.contains(key) ||
             prev[key] != iter.value()) {
            diff[key] = iter.value();
        }
        iter++;
    }

    return diff;
}

QSDiffRunner::QSDiffRunner()
{

}

QString QSDiffRunner::keyField() const
{
    return m_keyField;
}

void QSDiffRunner::setKeyField(const QString &keyField)
{
    m_keyField = keyField;
}

/*! \fn QList<QSChange> QSDiffRunner::run(const QVariantList &previous, const QVariantList &current)

    Call this function to compare two list, then return a
    list of patches required to transform from previous to current with
    the minimum number of steps. It uses an algorithm with O(n) runtime.
 */

QList<QSPatch> QSDiffRunner::compare(const QVariantList &previous, const QVariantList &current)
{
    QList<QSPatch> res;
    QList<QSPatch> updates;
    QVariantList prevList;
    QVariantList tmp;

    QHash<QString, int> currentHashTable;
    QHash<QString, int> prevHashTable;
    QVariantMap item;
    int offset = 0;

    currentHashTable.reserve(current.size() + 10);
    prevHashTable.reserve(previous.size() + 10);

    /* Step 1 - Check Removal */
    for (int i = 0 ; i < current.size() ; i++) {
        item = current.at(i).toMap();
        QString key = item[m_keyField].toString();
        currentHashTable[key] = i;
    }

    prevList = previous;

    tmp.clear();
    for (int i = prevList.size() - 1 ; i >= 0 ; i--) {
        item = prevList.at(i).toMap();
        QString key = item[m_keyField].toString();

        if (!currentHashTable.contains(key)) {
            res << QSPatch(QSPatch::Remove,
                           i, i, 1);
            prevList.removeAt(i); //@FIXME
        }
    }

    /* Step 2 - Compare to find move and update */

    // Build index table
    for (int i = 0 ; i < prevList.size() ; i++) {
        item = prevList.at(i).toMap();
        QString key = item[m_keyField].toString();

        prevHashTable[key] = i;
    }

    for (int i = 0 ; i < current.size() ; i++) {
        item = current.at(i).toMap();
        QString key = item[m_keyField].toString();

        if (!prevHashTable.contains(key)) {
            offset++;
            res << QSPatch(QSPatch::Insert, i, i, 1, item);
        } else {
            int prevPos = prevHashTable[key];
            int expectedPos = prevPos + offset;

            if (expectedPos != i) {
                QSPatch change(QSPatch::Move, prevPos, i, 1);
                res << change;

                offset++;
            }

            QVariantMap before = prevList.at(prevPos).toMap();
            QVariantMap after = current.at(i).toMap();
            QVariantMap diff = compareMap(before, after);
            if (diff.size() > 0) {
                updates << QSPatch(QSPatch::Update, i, i, 1, diff);
            }
        }
    }

    res = merge(res);

    if (updates.size() > 0) {
        res.append(updates);
    }

    return res;
}
