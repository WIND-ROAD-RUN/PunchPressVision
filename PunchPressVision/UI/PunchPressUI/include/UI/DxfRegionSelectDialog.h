#pragma once

#include <QDialog>
#include <QPoint>
#include <QRectF>

#include <vector>

#include "halconcpp/HalconCpp.h"
#include "UI/HalconInteractiveLabel.h"
#include "infrastructure/StampPatternModule/DxfPatternRenderer.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class DlgDxfRegionSelectClass; }
QT_END_NAMESPACE

namespace ui
{
	/// <summary>
	/// DXF 区域选择对话框。
	/// 用于一份 DXF 图纸中包含多个图案的场景：用户在预览上框选需要的图案区域
	/// （可多个），可逐个撤回；确定后以所有选中轮廓的中心绘制红十字并生成套版。
	/// 未绘制任何区域直接确定 = 使用全部轮廓。
	/// exec() 返回 Accepted 后，用 selectedContours() 取选中轮廓。
	/// </summary>
	class DxfRegionSelectDialog : public QDialog
	{
		Q_OBJECT

	public:
		explicit DxfRegionSelectDialog(const QString& dxfPath, QWidget* parent = nullptr);
		~DxfRegionSelectDialog() override;

		/// <summary>用户确认后：选中的轮廓（未绘制区域 = 全部轮廓）。</summary>
		HalconCpp::HObject selectedContours() const { return selectedContours_; }

	protected:
		void showEvent(QShowEvent* event) override;
		bool eventFilter(QObject* obj, QEvent* event) override;

	private slots:
		void onDrawToggled(bool checked);
		void onUndo();
		void onConfirm();
		void onCancel();

	private:
		struct RegionEntry
		{
			inf::DxfRect contourRect;   // 轮廓坐标系（用于过滤）
			QRectF imageRect;           // 图像坐标系（用于显示，x=col y=row）
		};

		bool loadAndPreview();
		void redrawOverlays();
		QRectF imageRectFromWidget(const QPoint& a, const QPoint& b) const;
		inf::DxfRect contourRectFromImage(const QRectF& imageRect) const;

		Ui::DlgDxfRegionSelectClass* ui;
		QString dxfPath_;

		// 预览控件（动态创建，替换 .ui 中的 QLabel 占位）
		HalconInteractiveLabel* labelPreview_{ nullptr };

		HalconCpp::HObject contours_;          // 全部轮廓（已定向）
		HalconCpp::HObject selectedContours_;  // 确认后的选中轮廓
		HalconCpp::HImage previewImage_;       // 预览图（黑底蓝线）
		HalconCpp::HTuple img2Contour_;        // 图像坐标 -> 轮廓坐标
		bool layoutReady_{ false };

		std::vector<RegionEntry> regions_;

		// 框选状态
		bool drawMode_{ false };
		bool dragging_{ false };
		QPoint dragStart_;
		QPoint dragCur_;
	};
}
