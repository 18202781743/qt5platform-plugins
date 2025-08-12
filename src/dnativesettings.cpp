// SPDX-FileCopyrightText: 2017 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dnativesettings.h"
#ifdef Q_OS_LINUX
#include "dxcbxsettings.h"
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#if __has_include("dplatformintegration.h")
#define __D_HAS_DPLATFORMINTEGRATION__
#endif
#else
#if QT_HAS_INCLUDE("dplatformintegration.h")
#define __D_HAS_DPLATFORMINTEGRATION__
#endif
#endif

#ifdef __D_HAS_DPLATFORMINTEGRATION__
#include "dplatformintegration.h"
#define IN_DXCB_PLUGIN
#endif

#include <QDebug>
#include <QMetaProperty>
#include <QMetaMethod>
#include <QLoggingCategory>

#define VALID_PROPERTIES "validProperties"
#define ALL_KEYS "allKeys"

Q_DECLARE_LOGGING_CATEGORY(dplatform)

DPP_BEGIN_NAMESPACE

QHash<QObject*, DNativeSettings*> DNativeSettings::mapped;
/*
 * 通过覆盖QObject的qt_metacall虚函数，检测base object中自定义的属性列表，将xwindow对应的设置和object对象中的属性绑定到一起使用
 * 将对象通过property/setProperty调用对属性的读写操作转为对xsetting的属性设 置
 */
DNativeSettings::DNativeSettings(QObject *base, DPlatformSettings *settings, bool global_settings)
    : m_base(base)
    , m_settings(settings)
    , m_isGlobalSettings(global_settings)
{
    qCDebug(dplatform) << "DNativeSettings constructor called, base:" << base << "global_settings:" << global_settings;
    if (mapped.value(base)) {
        qCCritical(dplatform) << "DNativeSettings: Native settings are already initialized for object:" << base;
        std::abort();
    }

    mapped[base] = this;

    const QMetaObject *meta_object;

    if (qintptr ptr = qvariant_cast<qintptr>(m_base->property("_d_metaObject"))) {
        meta_object = reinterpret_cast<const QMetaObject*>(ptr);
    } else {
        meta_object = m_base->metaObject();
    }

    if (m_settings->initialized()) {
        init(meta_object);
    }
}

DNativeSettings::~DNativeSettings()
{
    qCDebug(dplatform) << "DNativeSettings destructor called";
    if (!m_isGlobalSettings) {
        delete m_settings;
    } else if (
#ifdef IN_DXCB_PLUGIN
        DPlatformIntegration::instance() &&
#endif
        m_settings->initialized()
        ) {
        // 移除注册的callback
        m_settings->removeCallbackForHandle(this);
        m_settings->removeSignalCallback(this);
    }

    mapped.remove(m_base);

    if (m_metaObject) {
        free(m_metaObject);
    }
}

bool DNativeSettings::isValid() const
{
    bool result = m_settings->initialized();
    qCDebug(dplatform) << "isValid called, result:" << result;
    return result;
}

// TODO: This class needs to add a unit test
void DNativeSettings::init(const QMetaObject *metaObject)
{
    qCDebug(dplatform) << "init called, metaObject:" << metaObject;
    m_objectBuilder.addMetaObject(metaObject);
    m_firstProperty = metaObject->propertyOffset();
    m_propertyCount = m_objectBuilder.propertyCount();
    // 用于记录属性是否有效的属性, 属性类型为64位整数，最多可用于记录64个属性的状态
    m_flagPropertyIndex = metaObject->indexOfProperty(VALID_PROPERTIES);
    qint64 validProperties = 0;
    // 用于记录所有属性的key
    m_allKeysPropertyIndex = metaObject->indexOfProperty(ALL_KEYS);
    int allKeyPropertyTyep = 0;

    QMetaObjectBuilder &ob = m_objectBuilder;

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    ob.setFlags(ob.flags() | DynamicMetaObject);
#else
    ob.setFlags(ob.flags() | QMetaObjectBuilder::DynamicMetaObject);
#endif

    // 先删除所有的属性，等待重构
    qCDebug(dplatform) << "Removing all existing properties from object builder";
    while (ob.propertyCount() > 0) {
        ob.removeProperty(0);
    }

    QVector<int> propertySignalIndex;
    propertySignalIndex.reserve(m_propertyCount);

    // QMetaObjectBuilder对象中的属性、信号、方法均从0开始，但是m_base对象的QMetaObject则包含offset
    // 因此往QMetaObjectBuilder对象中添加属性时要将其对应的信号的index减去偏移量
    int signal_offset = metaObject->methodOffset();

    for (int i = 0; i < m_propertyCount; ++i) {
        int index = i + m_firstProperty;

        const QMetaProperty &mp = metaObject->property(index);
        qCDebug(dplatform) << "Processing property:" << mp.name() << "at index:" << index;

        if (mp.hasNotifySignal()) {
            qCDebug(dplatform) << "Property has notify signal, adding to signal index list";
            propertySignalIndex << mp.notifySignalIndex();
        }

        // 跳过特殊属性
        if (index == m_flagPropertyIndex) {
            qCDebug(dplatform) << "Skipping flag property at index:" << index;
            ob.addProperty(mp);
            continue;
        }

        if (index == m_allKeysPropertyIndex) {
            qCDebug(dplatform) << "Skipping all keys property at index:" << index;
            ob.addProperty(mp);
            allKeyPropertyTyep = mp.userType();
            continue;
        }

        if (m_settings->setting(mp.name()).isValid()) {
            qCDebug(dplatform) << "Setting found for property:" << mp.name();
            validProperties |= (1 << i);
        }

        QMetaPropertyBuilder op;

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        switch (mp.typeId()) {
#else
        switch (static_cast<int>(mp.type())) {
#endif
        case QMetaType::QByteArray:
        case QMetaType::QString:
        case QMetaType::QColor:
        case QMetaType::Int:
        case QMetaType::Double:
        case QMetaType::Bool:
            qCDebug(dplatform) << "Adding property with original type:" << mp.typeName();
            op = ob.addProperty(mp);
            break;
        default:
            // 重设属性的类型，只支持Int double color string bytearray
            qCDebug(dplatform) << "Converting property to QByteArray type:" << mp.typeName();
            op = ob.addProperty(mp.name(), "QByteArray", mp.notifySignalIndex() - signal_offset);
            break;
        }

        if (op.isWritable()) {
            // 声明支持属性reset
            qCDebug(dplatform) << "Setting property as resettable";
            op.setResettable(true);
        }
    }

    {
        // 通过class info确定是否应该关联对象的信号
        int index = metaObject->indexOfClassInfo("SignalType");
        qCDebug(dplatform) << "Checking for SignalType class info, index:" << index;

        if (index >= 0) {
            const QByteArray signals_value(metaObject->classInfo(index).value());
            qCDebug(dplatform) << "SignalType value:" << signals_value;

                    // 如果base对象声明为信号的生产者，则应该将其产生的信号转发到native settings
        if (signals_value == "producer") {
            qCDebug(dplatform) << "Object is signal producer, adding relay slot";
            // 创建一个槽用于接收所有信号
            m_relaySlotIndex = ob.addMethod("relaySlot(QByteArray,qint32,qint32)").index() + metaObject->methodOffset();
        }
        }
    }

    // 将属性状态设置给对象
    qCDebug(dplatform) << "Setting valid properties flag:" << validProperties;
    m_base->setProperty(VALID_PROPERTIES, validProperties);

    // 将所有属性名称设置给对象
    if (allKeyPropertyTyep == qMetaTypeId<QSet<QByteArray>>()) {
        qCDebug(dplatform) << "Setting all keys as QSet<QString>";
        QSet<QString> set(m_settings->settingKeys().begin(), m_settings->settingKeys().end());
        m_base->setProperty(ALL_KEYS, QVariant::fromValue(set));
    } else {
        qCDebug(dplatform) << "Setting all keys as QByteArrayList";
        m_base->setProperty(ALL_KEYS, QVariant::fromValue(m_settings->settingKeys()));
    }

    m_propertySignalIndex = metaObject->indexOfMethod(QMetaObject::normalizedSignature("propertyChanged(const QByteArray&, const QVariant&)"));
    qCDebug(dplatform) << "Property signal index:" << m_propertySignalIndex;
    // 监听native setting变化
    qCDebug(dplatform) << "Registering property change callback";
    m_settings->registerCallback(reinterpret_cast<DPlatformSettings::PropertyChangeFunc>(onPropertyChanged), this);
    // 监听信号. 如果base对象声明了要转发其信号，则此对象不应该关心来自于native settings的信号
    // 即信号的生产者和消费者只能选其一
    if (!isRelaySignal()) {
        qCDebug(dplatform) << "Registering signal callback";
        m_settings->registerSignalCallback(reinterpret_cast<DPlatformSettings::SignalFunc>(onSignal), this);
    } else {
        qCDebug(dplatform) << "Skipping signal callback registration (is relay signal)";
    }
    // 支持在base对象中直接使用property/setProperty读写native属性
    qCDebug(dplatform) << "Setting up meta object override";
    QObjectPrivate *op = QObjectPrivate::get(m_base);
    op->metaObject = this;
    m_metaObject = ob.toMetaObject();
    *static_cast<QMetaObject *>(this) = *m_metaObject;

    if (isRelaySignal()) {
        qCDebug(dplatform) << "Setting up relay signal connections";
        // 把 static_metacall 置为nullptr，迫使对base对象调用QMetaObject::invodeMethod时使用DNativeSettings::metaCall
        d.static_metacall = nullptr;
        // 链接 base 对象的所有信号
        int first_method = methodOffset();
        int method_count = methodCount();
        qCDebug(dplatform) << "Connecting signals, method count:" << method_count;

        for (int i = 0; i < method_count; ++i) {
            int index = i + first_method;

            // 排除属性对应的信号
            if (propertySignalIndex.contains(index)) {
                qCDebug(dplatform) << "Skipping property signal at index:" << index;
                continue;
            }

            QMetaMethod method = this->method(index);

            if (method.methodType() != QMetaMethod::Signal) {
                qCDebug(dplatform) << "Skipping non-signal method at index:" << index;
                continue;
            }

            qCDebug(dplatform) << "Connecting signal:" << method.name() << "at index:" << index;
            QMetaObject::connect(m_base, index, m_base, m_relaySlotIndex, Qt::DirectConnection);
        }
    }
}

QByteArray DNativeSettings::getSettingsProperty(QObject *base)
{
    qCDebug(dplatform) << "getSettingsProperty called, base:" << base;
    const QMetaObject *meta_object;

    if (qintptr ptr = qvariant_cast<qintptr>(base->property("_d_metaObject"))) {
        qCDebug(dplatform) << "Using custom meta object from property";
        meta_object = reinterpret_cast<const QMetaObject*>(ptr);
    } else {
        qCDebug(dplatform) << "Using default meta object";
        meta_object = base->metaObject();
    }

    QByteArray settings_property;

    {
        // 获取base对象是否指定了native settings的域
        // 默认情况下，native settings的值保存在窗口的_XSETTINGS_SETTINGS属性上
        // 指定域后，会将native settings的值保存到指定的窗口属性。
        // 将域的值转换成窗口属性时，会把 "/" 替换为 "_"，如域："/xxx/xxx" 转成窗口属性为："_xxx_xxx"
        // 且所有字母转换为大写
        settings_property = base->property("_d_domain").toByteArray();
        qCDebug(dplatform) << "Domain from property:" << settings_property;

        if (settings_property.isEmpty()) {
            qCDebug(dplatform) << "No domain property, checking class info";
            int index = meta_object->indexOfClassInfo("Domain");

            if (index >= 0) {
                settings_property = QByteArray(meta_object->classInfo(index).value());
                qCDebug(dplatform) << "Domain from class info:" << settings_property;
            }
        }

        if (!settings_property.isEmpty()) {
            qCDebug(dplatform) << "Converting domain to window property format";
            settings_property = settings_property.toUpper();
            settings_property.replace('/', '_');
            qCDebug(dplatform) << "Final settings property:" << settings_property;
        }
    }

    return settings_property;
}

int DNativeSettings::createProperty(const char *name, const char *)
{
    qCDebug(dplatform) << "createProperty called, name:" << name;
    // 不处理空字符串
    if (strlen(name) == 0) {
        qCDebug(dplatform) << "Empty property name, returning -1";
        return -1;
    }

    // 不创建特殊属性(以'_'开头的属性认为是私有的，不自动关联到native Settings)
    if (QByteArrayLiteral(VALID_PROPERTIES) == name
            || QByteArrayLiteral(ALL_KEYS) == name
            || name[0] == '_') {
        qCDebug(dplatform) << "Skipping special property:" << name;
        return -1;
    }

    // 清理旧数据
    qCDebug(dplatform) << "Freeing old meta object";
    free(m_metaObject);

    // 添加新属性
    qCDebug(dplatform) << "Adding new property:" << name;
    auto property = m_objectBuilder.addProperty(name, "QVariant");
    property.setReadable(true);
    property.setWritable(true);
    property.setResettable(true);
    m_metaObject = m_objectBuilder.toMetaObject();
    *static_cast<QMetaObject *>(this) = *m_metaObject;

    const auto &result = m_firstProperty + property.index();
    qCDebug(dplatform) << "Created property at index:" << result;
    return result;
}

void DNativeSettings::onPropertyChanged(const QByteArray &name, const QVariant &property, DNativeSettings *handle)
{
    qCDebug(dplatform) << "Property changed:" << name << "value:" << property;
    if (handle->m_propertySignalIndex >= 0) {
        qCDebug(dplatform) << "Invoking property changed signal";
        handle->method(handle->m_propertySignalIndex).invoke(handle->m_base, Q_ARG(QByteArray, name), Q_ARG(QVariant, property));
    }

    // 重设对象的 ALL_KEYS 属性
    {
        qCDebug(dplatform) << "Updating ALL_KEYS property";
        const QVariant &old_property = handle->m_base->property(ALL_KEYS);

        if (old_property.canConvert<QSet<QByteArray>>()) {
            qCDebug(dplatform) << "Processing ALL_KEYS as QSet<QByteArray>";
            QSet<QByteArray> keys = qvariant_cast<QSet<QByteArray>>(old_property);
            int old_count = keys.count();

            if (property.isValid()) {
                qCDebug(dplatform) << "Adding key to set:" << name;
                keys << name;
            } else if (keys.contains(name)) {
                qCDebug(dplatform) << "Removing key from set:" << name;
                keys.remove(name);
            }

            // 数量无变化时说明值无变化
            if (old_count != keys.count()) {
                qCDebug(dplatform) << "Keys count changed, updating property";
                handle->m_base->setProperty(ALL_KEYS, QVariant::fromValue(keys));
            } else {
                qCDebug(dplatform) << "Keys count unchanged, skipping update";
            }
        } else {
            qCDebug(dplatform) << "Processing ALL_KEYS as QByteArrayList";
            bool changed = false;
            QByteArrayList keys = qvariant_cast<QByteArrayList>(old_property);

            if (property.isValid()) {
                if (!keys.contains(name)) {
                    qCDebug(dplatform) << "Adding key to list:" << name;
                    keys << name;
                    changed = true;
                }
            } else if (keys.contains(name)) {
                qCDebug(dplatform) << "Removing key from list:" << name;
                keys.removeOne(name);
                changed = true;
            }

            if (changed) {
                qCDebug(dplatform) << "Keys list changed, updating property";
                handle->m_base->setProperty(ALL_KEYS, QVariant::fromValue(keys));
            } else {
                qCDebug(dplatform) << "Keys list unchanged, skipping update";
            }
        }
    }

    // 不要直接调用自己的indexOfProperty函数，属性不存在时会导致调用createProperty函数
    int property_index = handle->m_objectBuilder.indexOfProperty(name.constData());
    qCDebug(dplatform) << "Property index in object builder:" << property_index;

    if (Q_UNLIKELY(property_index < 0)) {
        qCDebug(dplatform) << "Property not found in object builder, skipping";
        return;
    }

    {
        bool ok = false;
        qint64 flags = handle->m_base->property(VALID_PROPERTIES).toLongLong(&ok);
        qCDebug(dplatform) << "Current valid properties flags:" << flags << "ok:" << ok;
        // 更新有效属性的标志位
        if (ok) {
            qint64 flag = (1 << property_index);
            flags = property.isValid() ? flags | flag : flags & ~flag;
            qCDebug(dplatform) << "Updated valid properties flags:" << flags;
            handle->m_base->setProperty(VALID_PROPERTIES, flags);
        }
    }

    const QMetaProperty &p = handle->property(handle->m_firstProperty + property_index);

    if (p.hasNotifySignal()) {
        // 通知属性改变
        qCDebug(dplatform) << "Invoking property notify signal";
        p.notifySignal().invoke(handle->m_base);
    }
}

// 处理native settings发过来的信号
void DNativeSettings::onSignal(const QByteArray &signal, qint32 data1, qint32 data2, DNativeSettings *handle)
{
    qCDebug(dplatform) << "Signal received:" << signal << "data1:" << data1 << "data2:" << data2;
    // 根据不同的参数寻找对应的信号
    static QByteArrayList signal_suffixs {
        QByteArrayLiteral("()"),
        QByteArrayLiteral("(qint32)"),
        QByteArrayLiteral("(qint32,qint32)")
    };

    int signal_index = -1;

    for (const QByteArray &suffix : signal_suffixs) {
        signal_index = handle->indexOfMethod(signal + suffix);
        qCDebug(dplatform) << "Checking signal with suffix:" << suffix << "index:" << signal_index;

        if (signal_index >= 0) {
            qCDebug(dplatform) << "Found signal method at index:" << signal_index;
            break;
        }
    }

    if (signal_index < 0) {
        qCDebug(dplatform) << "No matching signal method found";
        return;
    }

    QMetaMethod signal_method = handle->method(signal_index);
    qCDebug(dplatform) << "Invoking signal method:" << signal_method.name();
    // 调用base对象对应的信号
    signal_method.invoke(handle->m_base, Qt::DirectConnection, Q_ARG(qint32, data1), Q_ARG(qint32, data2));
}

int DNativeSettings::metaCall(QMetaObject::Call _c, int _id, void ** _a)
{
    qCDebug(dplatform) << "metaCall called, call type:" << _c << "id:" << _id;
    enum CallFlag {
        ReadProperty = 1 << QMetaObject::ReadProperty,
        WriteProperty = 1 << QMetaObject::WriteProperty,
        ResetProperty = 1 << QMetaObject::ResetProperty,
        AllCall = ReadProperty | WriteProperty | ResetProperty
    };

    if (AllCall & (1 << _c)) {
        qCDebug(dplatform) << "Processing property call";
        const QMetaProperty &p = property(_id);
        const int index = p.propertyIndex();
        qCDebug(dplatform) << "Property:" << p.name() << "index:" << index;
        // 对于本地属性，此处应该从m_settings中读写
        if (Q_LIKELY(index != m_flagPropertyIndex && index != m_allKeysPropertyIndex
                     && index >= m_firstProperty)) {
            qCDebug(dplatform) << "Processing native property";
            switch (_c) {
            case QMetaObject::ReadProperty:
                qCDebug(dplatform) << "Reading property:" << p.name();
                *reinterpret_cast<QVariant*>(_a[1]) = m_settings->setting(p.name());
                _a[0] = reinterpret_cast<QVariant*>(_a[1])->data();
                break;
            case QMetaObject::WriteProperty:
                qCDebug(dplatform) << "Writing property:" << p.name();
                m_settings->setSetting(p.name(), *reinterpret_cast<QVariant*>(_a[1]));
                break;
            case QMetaObject::ResetProperty:
                qCDebug(dplatform) << "Resetting property:" << p.name();
                m_settings->setSetting(p.name(), QVariant());
                break;
            default:
                qCDebug(dplatform) << "Unknown property call type:" << _c;
                break;
            }

            return -1;
        } else {
            qCDebug(dplatform) << "Skipping special property or out of range property";
        }
    }

    do {
        if (!isRelaySignal()) {
            qCDebug(dplatform) << "Not relay signal, skipping";
            break;
        }

        if (Q_LIKELY(_c != QMetaObject::InvokeMetaMethod || _id != m_relaySlotIndex)) {
            qCDebug(dplatform) << "Not relay slot call, call type:" << _c << "id:" << _id << "relay slot index:" << m_relaySlotIndex;
            break;
        }

        qCDebug(dplatform) << "Processing relay slot call";
        int signal = m_base->senderSignalIndex();
        QByteArray signal_name;
        qint32 data1 = 0, data2 = 0;

        // 不是通过信号触发的槽调用，可能是使用QMetaObject::invoke
        if (signal < 0) {
            qCDebug(dplatform) << "Direct invoke call";
            signal_name = *reinterpret_cast<QByteArray*>(_a[1]);
            data1 = *reinterpret_cast<qint32*>(_a[2]);
            data2 = *reinterpret_cast<qint32*>(_a[3]);
        } else {
            qCDebug(dplatform) << "Signal triggered call, signal index:" << signal;
            const auto &signal_method = method(signal);
            signal_name = signal_method.name();

            // 0为return type, 因此参数值下标从1开始
            if (signal_method.parameterCount() > 0) {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                QVariant arg(signal_method.parameterMetaType(0), _a[1]);
#else
                QVariant arg(signal_method.parameterType(0), _a[1]);
#endif
                // 获取参数1，获取参数2
                data1 = arg.toInt();
            }

            if (signal_method.parameterCount() > 1) {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                QVariant arg(signal_method.parameterMetaType(1), _a[2]);
#else
                QVariant arg(signal_method.parameterType(1), _a[2]);
#endif
                data2 = arg.toInt();
            }
        }

        qCDebug(dplatform) << "Emitting signal:" << signal_name << "data1:" << data1 << "data2:" << data2;
        m_settings->emitSignal(signal_name, data1, data2);

        return -1;
    } while (false);

    qCDebug(dplatform) << "Delegating to base metacall";
    return m_base->qt_metacall(_c, _id, _a);
}

bool DNativeSettings::isRelaySignal() const
{
    const auto &result = m_relaySlotIndex > 0;
    qCDebug(dplatform) << "isRelaySignal called, result:" << result << "relay slot index:" << m_relaySlotIndex;
    return result;
}

DPP_END_NAMESPACE
