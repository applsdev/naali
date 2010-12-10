/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   AssetsWindow.cpp
 *  @brief  
 *
 *          Detailed.
 */

#include "StableHeaders.h"
#include "AssetsWindow.h"
#include "DebugOperatorNew.h"

#include "Framework.h"
#include "AssetAPI.h"
#include "IAsset.h"
#include "IAssetStorage.h"

#include "MemoryLeakCheck.h"

/*class AssetItem: public QTreeWidgetItem
{
};
*/

namespace
{
    bool HasSameRefAsPredecessors(QTreeWidgetItem *item)
    {

        QTreeWidgetItem *parent = 0, *child = item;
        while((parent = child->parent()) != 0)
        {
            if (parent->text(0).compare(child->text(0), Qt::CaseInsensitive) == 0)
                return true;
            child = parent;
        }

        return false;
    }
}

AssetsWindow::AssetsWindow(Foundation::Framework *fw) :
    framework(fw)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Assets"));
    resize(300, 400);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    setLayout(layout);

//    QLabel *entityLabel = new QLabel(tr("The following entities will be created:"));

    treeWidget = new QTreeWidget;
    treeWidget->setHeaderHidden(true);
//    treeWidget ->setColumnCount(3);
//    treeWidget ->setHeaderLabels(QStringList(QStringList() << tr("Create") << tr("ID") << tr("Name")));
//    treeWidget ->header()->setResizeMode(QHeaderView::ResizeToContents);

    QHBoxLayout *hlayout= new QHBoxLayout;
    QLabel *searchLabel = new QLabel(tr("Search filter: "), this);
    QLineEdit *searchField = new QLineEdit(this);
    QPushButton *expandAndCollapseButton = new QPushButton(tr("Expand/collapse all"), this);
    hlayout->addWidget(searchLabel);
    hlayout->addWidget(searchField);
    hlayout->addWidget(expandAndCollapseButton);

    layout->addLayout(hlayout);
    layout->addWidget(treeWidget);

    connect(searchField, SIGNAL(textChanged(const QString &)), SLOT(Search(const QString &)));
    connect(expandAndCollapseButton, SIGNAL(clicked()), SLOT(ExpandOrCollapseAll()));

    PopulateTreeWidget();
}

AssetsWindow::~AssetsWindow()
{
    // Disable ResizeToContents, Qt goes sometimes into eternal loop after
    // ~AddContentWindow() if we have lots (hudreds or thousands) of items.
    treeWidget->header()->setResizeMode(QHeaderView::Interactive);

    QTreeWidgetItemIterator it(treeWidget);
    while(*it)
    {
        QTreeWidgetItem *item = *it;
        SAFE_DELETE(item);
        ++it;
    }
}

void AssetsWindow::AddChildren(const AssetPtr &asset, QTreeWidgetItem *parent, std::set<AssetPtr> &alreadyAdded)
{
    foreach(AssetReference ref, asset->FindReferences())
    {
        AssetPtr asset = framework->Asset()->GetAsset(ref.ref);
        if (asset && alreadyAdded.find(asset) == alreadyAdded.end())
        {
            QTreeWidgetItem *item= new QTreeWidgetItem(parent);
            item->setText(0, asset->Name());
            parent->addChild(item);
            alreadyAdded.insert(asset);

            // Check that we don't have 
            if (HasSameRefAsPredecessors(item))
                item->setText(0, tr("Recursive dependency to ") + asset->Name());
            else
                AddChildren(asset, item, alreadyAdded);
        }
    }
}

void AssetsWindow::PopulateTreeWidget()
{
    foreach(AssetStoragePtr storage, framework->Asset()->GetAssetStorages())
    {
        QTreeWidgetItem *item = new QTreeWidgetItem;
        item->setText(0, storage->Name());
        treeWidget->addTopLevelItem(item);
    }

    // Create "No provider" for assets without storage.
    QTreeWidgetItem *noProviderItem = new QTreeWidgetItem;
    noProviderItem->setText(0, tr("No provider"));
    treeWidget->addTopLevelItem(noProviderItem);

    std::set<AssetPtr> alreadyAdded;

    std::pair<QString, AssetPtr> pair;
    foreach(pair, framework->Asset()->GetAllAssets())
    {
        if (alreadyAdded.find(pair.second) != alreadyAdded.end())
            continue;

        QTreeWidgetItem *aitem = new QTreeWidgetItem;
        aitem->setText(0, pair.first);
        alreadyAdded.insert(pair.second);

        AddChildren(pair.second, aitem, alreadyAdded);

        AssetStoragePtr storage = pair.second->GetAssetStorage();
        bool storageFound = false;
        if (storage)
            for (int i = 0; i < treeWidget->topLevelItemCount(); ++i)
            {
                QTreeWidgetItem *storageItem = treeWidget->topLevelItem(i);
                if (storageItem->text(0) == storage->Name())
                {
                    storageItem->addChild(aitem);
                    storageFound = true;
                    break;
                }
            }

        if (!storageFound)
            noProviderItem->addChild(aitem);
    }

    if (noProviderItem->childCount() == 0)
        SAFE_DELETE(noProviderItem);
}

void AssetsWindow::Search(const QString &filter)
{
    QString f = filter.trimmed();
    bool expand = f.size() >= 3;
    QSet<QTreeWidgetItem *> alreadySetVisible;

    QTreeWidgetItemIterator it(treeWidget);
    while(*it)
    {
        QTreeWidgetItem *item = *it;
        if (!alreadySetVisible.contains(item))
        {
            if (f.isEmpty())
            {
                item->setHidden(false);
            }
            else if (item->text(0).contains(filter, Qt::CaseInsensitive))
            {
                item->setHidden(false);
                alreadySetVisible.insert(item);
                if (expand)
                    item->setExpanded(expand);

                // Make sure that all the parent items are visible too
                QTreeWidgetItem *parent = 0, *child = item;
                while((parent = child->parent()) != 0)
                {
                    parent->setHidden(false);
                    alreadySetVisible.insert(parent);
                    if (expand)
                        parent->setExpanded(expand);
                    child = parent;
                }
            }
            else
            {
                item->setHidden(true);
            }
        }

        ++it;
    }
}

void AssetsWindow::ExpandOrCollapseAll()
{
    bool expand = true;
    for (int i = 0; i < treeWidget->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = treeWidget->topLevelItem(i);
        if (item->childCount() >= 1 && item->isExpanded())
            expand = false;
    }

    if (expand)
        treeWidget->expandAll();
    else
        treeWidget->collapseAll();
}