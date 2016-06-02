#include <QFont>
#include "deconz.h"
#include "otau_file.h"
#include "otau_node.h"
#include "otau_model.h"
#include "std_otau_plugin.h"

/*! The constructor.
 */
OtauModel::OtauModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

/*! Returns the model rowcount.
 */
int OtauModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_nodes.size();
}

/*! Returns the model columncount.
 */
int OtauModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return SectionCount;
}

/*! Returns the model headerdata for a column.
 */
QVariant OtauModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(orientation);

    if (role == Qt::DisplayRole && (orientation == Qt::Horizontal))
    {
        switch (section)
        {
        case SectionAddress:
            return tr("Address");

        case SectionSoftwareVersion:
            return tr("Version");

        case SectionImageType:
            return tr("Image");

        case SectionProgress:
            return tr("Progress");

        case SectionDuration:
            return tr("Duration");

//        case SectionStatus:
//            return tr("Status");

        default:
            return tr("Unknown");
        }
    }

    return QVariant();
}

/*! Returns the model data for a specific column.
 */
QVariant OtauModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (index.row() >= rowCount(QModelIndex()))
        {
            return QVariant();
        }

        QString str;
        OtauNode *node = m_nodes[index.row()];

        switch(index.column())
        {
        case SectionAddress:
            if (node->address().hasExt())
            {
                str = node->address().toStringExt();
            }
            else if (node->address().hasNwk())
            {
                str = node->address().toStringNwk();
            }
            break;

        case SectionSoftwareVersion:
            str.sprintf("0x%08X", node->softwareVersion());
            break;

        case SectionImageType:
            str.sprintf("0x%04X", node->imageType());
            break;

        case SectionProgress:
            if (node->zclCommandId == OTAU_UPGRADE_END_RESPONSE_CMD_ID)
            {
                switch(node->upgradeEndReq.status)
                {
                case OTAU_SUCCESS:            str = tr("Done"); break;
                case OTAU_ABORT:              str = tr("Abort"); break;
                case OTAU_INVALID_IMAGE:      str = tr("Invalid image"); break;
                case OTAU_REQUIRE_MORE_IMAGE: str = tr("Require more image"); break;
                default:
                    str = tr("Unknown");
                    break;
                }
            }
            else if (node->zclCommandId == OTAU_QUERY_NEXT_IMAGE_RESPONSE_CMD_ID)
            {
                if (node->hasData())
                {
                    str = tr("Idle");
                }
                else
                {
                    str = tr("No file");
                }
            }
            else if (node->permitUpdate())
            {
                if (node->offset() > 0)
                {
                    if (node->offset() == node->file.totalImageSize)
                    {
                        str = tr("Done");
                    }
                    else
                    {
                        str.sprintf("%3.2f%%", ((double)node->offset() / (double)node->file.totalImageSize) * 100.0f);
                    }
                }
                else
                {
                    str = tr("Queued");
                }
            }
            else if (node->hasData())
            {
                str = tr("Paused");
            }
            else
            {
                str = tr("No file");
            }
            break;

        case SectionDuration:
        {
            int min = (node->elapsedTime() / 1000) / 60;
            int sec = (node->elapsedTime() / 1000) % 60;
            str.sprintf("%u:%02u", min, sec);
        }
            break;

//        case SectionStatus:
//            str = node->statusString();
//            break;

        default:
            break;
        }

        return str;
    }
    else if (role == Qt::ToolTipRole)
    {
        if (index.row() >= rowCount(QModelIndex()))
        {
            return QVariant();
        }

        QString str;
        OtauNode *node = m_nodes[index.row()];

        switch (index.column())
        {
        case SectionSoftwareVersion:
            // dresden electronic spezific version format
            if (node->softwareVersion() != 0)
            {
                if ((node->address().ext() & 0x00212EFFFF000000ULL) != 0)
                {
                    str.sprintf("%u.%u build %u", (node->softwareVersion() & 0xF0000000U) >> 28, (node->softwareVersion() & 0x0FF00000U) >> 20, (node->softwareVersion() & 0x000FFFFFU));
                    return str;
                }
            }
            break;

        default:
            break;
        }
    }
    else if (role == Qt::FontRole)
    {
        switch (index.column())
        {
        case SectionAddress:
        case SectionSoftwareVersion:
        case SectionImageType:
        {
            QFont font("Monospace");
            font.setStyleHint(QFont::TypeWriter);
            return font;
        }

        default:
            break;
        }
    }

    return QVariant();
}

/*! Returns a OtauNode.
    \param addr - the nodes address which must contain nwk and ext address
    \param create - true if a OtauNode shall be created if it does not exist yet
    \return pointer to a OtauNode or 0 if not found
 */
OtauNode *OtauModel::getNode(const deCONZ::Address &addr, bool create)
{
    std::vector<OtauNode*>::iterator i = m_nodes.begin();
    std::vector<OtauNode*>::iterator end = m_nodes.end();

    if (!addr.hasExt() && !addr.hasNwk())
    {
        return 0;
    }

    for (; i != end; ++i)
    {
        if (addr.hasExt() && (*i)->address().hasExt())
        {
            if ((*i)->address().ext() == addr.ext())
            {
                if ((*i)->address().nwk() != addr.nwk())
                {
                    // update nwk address
                }
                return *i;
            }
        }

        if (addr.hasNwk() && (*i)->address().hasNwk())
        {
            if ((*i)->address().nwk() == addr.nwk())
            {
                return *i;
            }
        }
    }

    if (create && addr.hasExt() && addr.hasNwk())
    {
        // not found create new
        uint row = m_nodes.size();

        beginInsertRows(QModelIndex(), row, row);
        OtauNode *node = new OtauNode(addr);
        node->row = row;
        node->model = this;
        m_nodes.push_back(node);
        endInsertRows();
        DBG_Printf(DBG_INFO, "OtauNode added %s\n", qPrintable(addr.toStringExt()));
        return node;
    }

    return 0;
}

/*! Returns a OtauNode for a given row.
 */
OtauNode *OtauModel::getNodeAtRow(uint row)
{
    if (m_nodes.size() > row)
    {
        return m_nodes[row];
    }

    return 0;
}

/*! Notify model/view that the data for a given node has changed.
 */
void OtauModel::nodeDataUpdate(OtauNode *node)
{
    if (node && (node->row < m_nodes.size()))
    {
        emit dataChanged(index(node->row, 0), index(node->row, SectionCount));
    }
}

/*! Returns the internal vector of nodes.
 */
std::vector<OtauNode *> &OtauModel::nodes()
{
    return m_nodes;
}