#pragma once

#include <QObject>
#include <QThread>
#include <atomic>
#include <memory>
#include <QString>
#include "render_data.h"
#include "../mesh/mesh.h"

class MeshWorker : public QObject {
    Q_OBJECT
public:
    explicit MeshWorker(QObject* parent = nullptr) : QObject(parent) {}

    void requestCancel() { m_cancelled.store(true); }

signals:
    void progress(int percent, QString stage);  // убрать?
    // void startWork(const QString& path); 
    void finished(RenderDataPtr data);
    void failed(QString reason);
    // void loadFailed(const QString& reason);

public slots:
    void process(const QString& stlPath);

private:
    std::atomic<bool> m_cancelled{false};

    // Кэш для повторных вызовов (смена параметров визуализации в будущем)
    std::unique_ptr<Mesh> m_cachedMesh;
    QString               m_cachedPath;

    void buildRenderData(const Mesh& mesh, RenderData& out);
};