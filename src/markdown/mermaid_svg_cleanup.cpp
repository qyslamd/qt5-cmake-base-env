#include "mermaid_svg_cleanup.h"

#include <cmath>

#include <QHash>
#include <QPointF>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

namespace {

// mermaid 的字号默认 16px（主题 CSS 里写死），tspan 上的 em 单位按它换算
constexpr double kDefaultFontSize = 16.0;

QString AttrOf(const QString& tag, const QString& name) {
  const QRegularExpression re(
      QStringLiteral("\\b%1\\s*=\\s*\"([^\"]*)\"").arg(name));
  const QRegularExpressionMatch match = re.match(tag);
  return match.hasMatch() ? match.captured(1) : QString();
}

// 解析 SVG <length>：认普通数值与 em。认不出来返回 false，调用方保持原值
bool ParseLen(const QString& value, double font_size, double* out) {
  static const QRegularExpression kLen(
      QStringLiteral("^\\s*(-?[\\d.]+)\\s*(em|px)?\\s*$"));
  const QRegularExpressionMatch match = kLen.match(value.trimmed());
  if (!match.hasMatch()) {
    return false;
  }
  const double v = match.captured(1).toDouble();
  *out = (match.captured(2) == QLatin1String("em")) ? v * font_size : v;
  return true;
}

bool ParseNumber(const QString& value, double* out) {
  bool ok = false;
  const double v = value.trimmed().toDouble(&ok);
  if (ok) {
    *out = v;
  }
  return ok;
}

// 剥掉定位属性，其余（class / font-weight / text-anchor）原样保留
QString StripPositionAttrs(const QString& tag) {
  QString out = tag;
  for (const QString& name : {QStringLiteral("x"), QStringLiteral("y"),
                              QStringLiteral("dx"), QStringLiteral("dy")}) {
    out.remove(QRegularExpression(
        QStringLiteral("\\s+%1\\s*=\\s*\"[^\"]*\"").arg(name)));
  }
  return out;
}

// 取出标签里的属性串（不含 <name 与结尾的 > 或 />）：存属性就只存属性，
// 拼回去时才不会出现 "<tspan<tspan …" 这种非法结构
QString TagAttrs(const QString& tag) {
  const int start = tag.indexOf(QChar(' '));
  if (start < 0) {
    return QString();
  }
  const int end = tag.lastIndexOf(QChar('>'));
  if (end <= start) {
    return QString();
  }
  return tag.mid(start, end - start);
}

// 合并两个开标签的属性（后者覆盖同名），保持出现顺序：重复属性会让 XML
// 解析直接报错，不能简单拼接
QString MergeTags(const QString& base_tag, const QString& over_tag) {
  QStringList order;
  QHash<QString, QString> attrs;
  static const QRegularExpression kAttr(
      QStringLiteral("([a-zA-Z_:][-\\w:.]*)\\s*=\\s*\"([^\"]*)\""));
  for (const QString& tag : {base_tag, over_tag}) {
    auto it = kAttr.globalMatch(tag);
    while (it.hasNext()) {
      const QRegularExpressionMatch match = it.next();
      const QString name = match.captured(1);
      if (!attrs.contains(name)) {
        order.append(name);
      } else if (name == QLatin1String("style")) {
        attrs[name] += QChar(';') + match.captured(2);
        continue;
      }
      attrs[name] = match.captured(2);
    }
  }
  QString out;
  for (const QString& name : order) {
    out += QChar(' ') + name + QStringLiteral("=\"") + attrs.value(name) +
           QChar('"');
  }
  return out;
}

// ---- 文本定标 ----
//
// mermaid 把真正的基线写在定位 tspan 上（<text y="-10.1"><tspan x="0"
// y="-0.1em" dy="1.1em">）。按 SVG 规范 x/y 是绝对覆盖、dx/dy 相对累加，
// 而 Qt 的 QSvgTspan 不存这四个属性，一个 <text> 下所有 tspan 都被当同一行画
// 在 <text> 自己的坐标上——不烘焙就会整体偏高约 24 单位。

struct TextPos {
  double x = 0.0;
  double y = 0.0;
};

void AccumulateAttrs(const QString& tag, double font_size, TextPos* pos) {
  double v = 0.0;
  if (ParseLen(AttrOf(tag, QStringLiteral("x")), font_size, &v)) {
    pos->x = v;
  }
  if (ParseLen(AttrOf(tag, QStringLiteral("y")), font_size, &v)) {
    pos->y = v;
  }
  if (ParseLen(AttrOf(tag, QStringLiteral("dx")), font_size, &v)) {
    pos->x += v;
  }
  if (ParseLen(AttrOf(tag, QStringLiteral("dy")), font_size, &v)) {
    pos->y += v;
  }
}

double FontSizeOf(const QString& tag, double fallback) {
  double px = 0.0;
  if (ParseLen(AttrOf(tag, QStringLiteral("font-size")), fallback, &px) &&
      px > 0.0) {
    return px;
  }
  static const QRegularExpression kCss(
      QStringLiteral("font-size\\s*:\\s*([\\d.]+)\\s*(em|px)?"));
  const QRegularExpressionMatch match =
      kCss.match(AttrOf(tag, QStringLiteral("style")));
  if (match.hasMatch()) {
    const double v = match.captured(1).toDouble();
    return (match.captured(2) == QLatin1String("em")) ? v * fallback : v;
  }
  return fallback;
}

// 行内容里的嵌套 tspan 只留样式（位置已经烘焙到 <text> 上了）
QString StripNestedPositions(const QString& content) {
  static const QRegularExpression kTspan(QStringLiteral("<tspan\\b([^>]*)>"));
  QString out;
  int pos = 0;
  auto it = kTspan.globalMatch(content);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    out += content.mid(pos, match.capturedStart() - pos);
    pos = match.capturedEnd();
    out += QStringLiteral("<tspan") + StripPositionAttrs(match.captured(1)) +
           QChar('>');
  }
  out += content.mid(pos);
  return out;
}

struct RowSpan {
  QString open_tag;  // 只存属性串
  QString content;
  QString raw;  // 整段（含 tspan 外壳）
};

// 切出 <text> 里的顶层 tspan（mermaid 每行一个，内部再套 run）
QVector<RowSpan> TopLevelTspans(const QString& inner) {
  // 第一个候选连自闭合的 <tspan .../> 一起吃（[^>]* 会把结尾的 / 一并吞掉），
  // 是不是自闭合由下面的 endsWith("/>") 判
  static const QRegularExpression kTag(
      QStringLiteral("<tspan\\b[^>]*/?>|</tspan>"));
  QVector<RowSpan> rows;
  int depth = 0;
  int content_start = -1;
  int row_start = -1;
  QString open_tag;
  auto it = kTag.globalMatch(inner);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    const QString token = match.captured(0);
    if (token.startsWith(QLatin1String("</"))) {
      if (depth == 0) {
        continue;
      }
      if (--depth == 0) {
        RowSpan row;
        row.open_tag = open_tag;
        row.content =
            inner.mid(content_start, match.capturedStart() - content_start);
        row.raw = inner.mid(row_start, match.capturedEnd() - row_start);
        rows.append(row);
      }
      continue;
    }
    if (token.endsWith(QLatin1String("/>"))) {
      if (depth == 0) {
        RowSpan row;
        row.open_tag = TagAttrs(token);
        row.raw = token;
        rows.append(row);
      }
      continue;
    }
    if (depth == 0) {
      row_start = match.capturedStart();
      content_start = match.capturedEnd();
      open_tag = TagAttrs(token);
    }
    ++depth;
  }
  return rows;
}

// 行的正文。两条硬规矩：Qt 只接受 tspan 作为 <text> 的直接孩子，嵌套会报
// "Could not add child element ... types are incorrect"（所以行里已经有 run
// tspan 就摊平）；而外壳又不能丢——CSS 常按 "text.actor>tspan" 定位，少一层
// 规则就匹配不上，actor 名字会退回盒子的填充色（所以叶子行要包一层）
QString RowBody(const RowSpan& row) {
  const QString body = StripNestedPositions(row.content);
  if (row.content.contains(QLatin1String("<tspan"))) {
    return body;
  }
  return QStringLiteral("<tspan") + StripPositionAttrs(row.open_tag) +
         QChar('>') + body + QStringLiteral("</tspan>");
}

QString BakeTextElement(const QString& open_tag, const QString& inner) {
  const double font_size = FontSizeOf(open_tag, kDefaultFontSize);
  TextPos base;
  AccumulateAttrs(open_tag, font_size, &base);
  const QString head = StripPositionAttrs(open_tag);

  // xml:space="preserve"：mermaid 把 run 之间的空格写在后一个 run 开头
  // （" " + content），Qt 默认空白模式会吃掉它，"+String name" 会变成
  // "+Stringname"
  const QString base_attrs = head + QStringLiteral(" xml:space=\"preserve\"");
  auto build_text = [&](const TextPos& pos, const QString& over_attrs,
                        const QString& body) {
    return QStringLiteral("<text") + MergeTags(base_attrs, over_attrs) +
           QStringLiteral(" x=\"%1\" y=\"%2\">").arg(pos.x).arg(pos.y) + body +
           QStringLiteral("</text>");
  };

  const QVector<RowSpan> rows = TopLevelTspans(inner);
  if (rows.isEmpty()) {
    return build_text(base, QString(), inner);
  }
  QString out;
  QString covered;
  for (const RowSpan& row : rows) {
    TextPos pos = base;
    if (!row.open_tag.isEmpty()) {
      AccumulateAttrs(row.open_tag, font_size, &pos);
    }
    out += build_text(pos, StripPositionAttrs(row.open_tag), RowBody(row));
    covered += row.raw;
  }
  // 兜底：tspan 之外还挂着散字（mermaid 现在不这么写，丢了就是静默掉字）
  QString residual = inner;
  residual.remove(covered);
  if (!residual.trimmed().isEmpty()) {
    out += build_text(base, QString(), residual);
  }
  return out;
}

QString BakeTextPositions(const QString& svg) {
  static const QRegularExpression kText(
      QStringLiteral("<text\\b([^>]*)>(.*?)</text>"),
      QRegularExpression::DotMatchesEverythingOption);
  QString out;
  int pos = 0;
  auto it = kText.globalMatch(svg);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    out += svg.mid(pos, match.capturedStart() - pos);
    pos = match.capturedEnd();
    out += BakeTextElement(match.captured(1), match.captured(2));
  }
  out += svg.mid(pos);
  return out;
}

// ---- marker 内联 ----
//
// 展开成 <g transform="translate(端点) rotate(方向) scale(viewBox→viewport)
// translate(-refX,-refY)">内容</g>，几何按 marker 的真实定义算，不硬编码三角形

struct MarkerDef {
  QString attrs;  // 开标签属性（CSS 内联后的样式已经在里面）
  QString content;
  double ref_x = 0.0;
  double ref_y = 0.0;
  double vb_w = 0.0;
  double vb_h = 0.0;
  bool has_view_box = false;
  double width = 3.0;  // markerWidth / markerHeight 的缺省值是 3
  double height = 3.0;
  bool stroke_units = true;  // markerUnits 缺省 strokeWidth
  bool auto_start_reverse = false;
};

QHash<QString, MarkerDef> ParseMarkers(const QString& svg) {
  static const QRegularExpression kMarker(
      QStringLiteral("<marker\\b([^>]*)>(.*?)</marker>"),
      QRegularExpression::DotMatchesEverythingOption);
  QHash<QString, MarkerDef> defs;
  auto it = kMarker.globalMatch(svg);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    const QString tag = match.captured(1);
    const QString id = AttrOf(tag, QStringLiteral("id"));
    if (id.isEmpty()) {
      continue;
    }
    MarkerDef def;
    // 原样留着：marker 上没有 x/y/dx/dy，该剥的 id/refX/refY/marker*/orient 等
    // 在展开时由 MarkerStyleAttrs 一并处理
    def.attrs = tag;
    def.content = match.captured(2);
    ParseNumber(AttrOf(tag, QStringLiteral("refX")), &def.ref_x);
    ParseNumber(AttrOf(tag, QStringLiteral("refY")), &def.ref_y);
    ParseNumber(AttrOf(tag, QStringLiteral("markerWidth")), &def.width);
    ParseNumber(AttrOf(tag, QStringLiteral("markerHeight")), &def.height);
    const QString units = AttrOf(tag, QStringLiteral("markerUnits"));
    def.stroke_units = units.isEmpty() || units == QLatin1String("strokeWidth");
    def.auto_start_reverse = AttrOf(tag, QStringLiteral("orient")) ==
                             QLatin1String("auto-start-reverse");

    static const QRegularExpression kViewBox(
        QStringLiteral("viewBox\\s*=\\s*\"([^\"]*)\""));
    const QRegularExpressionMatch vm = kViewBox.match(tag);
    if (vm.hasMatch()) {
      QStringList nums =
          vm.captured(1).split(QRegularExpression(QStringLiteral("[\\s,]+")));
      nums.removeAll(QString());
      if (nums.size() == 4) {
        def.vb_w = nums[2].toDouble();
        def.vb_h = nums[3].toDouble();
        def.has_view_box = def.vb_w > 0.0 && def.vb_h > 0.0;
      }
    }
    defs.insert(id, def);
  }
  return defs;
}

// 端点：<line> 直接读 x1/y1/x2/y2，<path>/<polyline> 从坐标对里取
QVector<QPointF> ShapePoints(const QString& tag) {
  QVector<QPointF> pts;
  static const QRegularExpression kPair(
      QStringLiteral("(-?[\\d.]+)[,\\s]+(-?[\\d.]+)"));

  if (tag.startsWith(QLatin1String("<line"))) {
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    if (ParseNumber(AttrOf(tag, QStringLiteral("x1")), &x1) &&
        ParseNumber(AttrOf(tag, QStringLiteral("y1")), &y1) &&
        ParseNumber(AttrOf(tag, QStringLiteral("x2")), &x2) &&
        ParseNumber(AttrOf(tag, QStringLiteral("y2")), &y2)) {
      pts.append(QPointF(x1, y1));
      pts.append(QPointF(x2, y2));
    }
    return pts;
  }

  const bool is_poly = tag.startsWith(QLatin1String("<polyline")) ||
                       tag.startsWith(QLatin1String("<polygon"));
  auto it = kPair.globalMatch(
      AttrOf(tag, is_poly ? QStringLiteral("points") : QStringLiteral("d")));
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    pts.append(
        QPointF(match.captured(1).toDouble(), match.captured(2).toDouble()));
  }
  return pts;
}

double StrokeWidthOf(const QString& tag) {
  double width = 1.0;
  if (ParseNumber(AttrOf(tag, QStringLiteral("stroke-width")), &width)) {
    return width;
  }
  static const QRegularExpression kCss(
      QStringLiteral("stroke-width\\s*:\\s*([\\d.]+)"));
  const QRegularExpressionMatch match =
      kCss.match(AttrOf(tag, QStringLiteral("style")));
  if (match.hasMatch()) {
    return match.captured(1).toDouble();
  }
  return 1.0;
}

// 只留样式类属性：id 会和别的引用重名，marker* / viewBox / orient 已经被消化
// 进 transform 了，挂在 <g> 上只会误导
QString MarkerStyleAttrs(const QString& tag) {
  static const QRegularExpression kDrop(QStringLiteral(
      "\\s+(id|refX|refY|markerWidth|markerHeight|markerUnits|orient|"
      "viewBox|preserveAspectRatio)\\s*=\\s*\"[^\"]*\""));
  QString out = tag;
  out.remove(kDrop);
  return out;
}

// anchor：参考点（refX/refY 对齐的那个顶点）；dir_in：该顶点处**路径前进
// 方向**——起点也要取前进方向，只有 orient="auto-start-reverse" 才翻 180°，
// 取反了箭头会画到路径另一侧
QString EmitMarker(const MarkerDef& def, const QPointF& anchor,
                   const QPointF& dir_in, bool reverse, double stroke_width) {
  const double len = std::hypot(dir_in.x(), dir_in.y());
  if (len < 1e-6) {
    return QString();
  }
  const QPointF dir = dir_in / len;
  constexpr double kPi = 3.14159265358979323846;  // MSVC 的 <cmath> 不给 M_PI
  double angle = std::atan2(dir.y(), dir.x()) * 180.0 / kPi;
  if (reverse) {
    angle += 180.0;
  }

  double sx = 1.0;
  double sy = 1.0;
  // 对齐平移只有 −ref：refX/refY 与 marker 内容同处 viewBox 坐标系，而
  // V(p)−V(ref) = scale·(p−ref)，viewBox 的平移项在差里相消——它只贡献
  // 缩放。把 vb 原点塞进平移会让 marker 整体偏移 −scale·vb
  double tx = def.ref_x;
  double ty = def.ref_y;
  if (def.has_view_box) {
    sx = def.width / def.vb_w;
    sy = def.height / def.vb_h;
  }
  if (def.stroke_units) {
    sx *= stroke_width;
    sy *= stroke_width;
  }

  return QStringLiteral(
             "<g%1 transform=\"translate(%2, %3) rotate(%4) scale(%5, %6) "
             "translate(%7, %8)\">%9</g>")
      .arg(MarkerStyleAttrs(def.attrs))
      .arg(anchor.x())
      .arg(anchor.y())
      .arg(angle)
      .arg(sx)
      .arg(sy)
      .arg(-tx)
      .arg(-ty)
      .arg(def.content);
}

}  // namespace

QString CleanSvg(const QString& svg) {
  QString out = svg;

  // 1) 删 edgeLabel 背景 rect：无尺寸的会被 QSvgRenderer 画成灰块/黑块
  static const QRegularExpression kBgRect(
      QStringLiteral("<rect\\b(?=[^>]*\\bclass=\"background\")[^>]*>"
                     "\\s*(?:</rect>)?"),
      QRegularExpression::DotMatchesEverythingOption);
  out.replace(kBgRect, QString());

  // 2) tspan 的位置烘焙回 <text>（Qt 不认 tspan 的 x/y/dy）
  out = BakeTextPositions(out);

  // 3) stroke-dasharray=0 会让 QSvgRenderer 什么都不画（mermaid 用 0 表示实线）
  out.replace(QStringLiteral("stroke-dasharray=\"0\""),
              QStringLiteral("stroke-dasharray=\"none\""));
  out.replace(QStringLiteral("stroke-dasharray:0;"),
              QStringLiteral("stroke-dasharray:none;"));
  out.replace(QStringLiteral("stroke-dasharray:0\""),
              QStringLiteral("stroke-dasharray:none\""));

  // marker 定义先留着：InlineSvgMarkers 还要读它，而它必须等 CSS 内联之后跑
  return out;
}

QString InlineSvgMarkers(const QString& svg) {
  const QHash<QString, MarkerDef> defs = ParseMarkers(svg);
  if (defs.isEmpty()) {
    return svg;
  }

  static const QRegularExpression kRef(QStringLiteral(
      "\\s+marker-(start|mid|end)\\s*=\\s*\"url\\(#([^\\)]+)\\)\""));
  static const QRegularExpression kShape(
      QStringLiteral("<(path|line|polyline|polygon)\\b[^>]*>"),
      QRegularExpression::DotMatchesEverythingOption);

  QString out;
  int pos = 0;
  auto it = kShape.globalMatch(svg);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    const QString tag = match.captured(0);
    if (!tag.contains(QLatin1String("marker-"))) {
      continue;
    }
    out += svg.mid(pos, match.capturedStart() - pos);
    pos = match.capturedEnd();

    QString cleaned = tag;
    QString arrows;
    const QVector<QPointF> pts = ShapePoints(tag);
    const double stroke_width = StrokeWidthOf(tag);

    auto ref_it = kRef.globalMatch(tag);
    while (ref_it.hasNext()) {
      const QRegularExpressionMatch ref = ref_it.next();
      const QString kind = ref.captured(1);
      const QString id = ref.captured(2);
      const bool start_side = kind == QLatin1String("start");
      // 定义找不到就只清引用，别留悬空 marker
      if (!defs.contains(id) || pts.size() < 2) {
        continue;
      }
      if (start_side) {
        arrows +=
            EmitMarker(defs.value(id), pts.first(), pts.at(1) - pts.first(),
                       defs.value(id).auto_start_reverse, stroke_width);
      } else if (kind == QLatin1String("mid")) {
        // 中间拐点：每个都画一个，方向取入/出两段的角平分线（SVG 规范是
        // 两方向单位向量之和）。auto-start-reverse 在 mid 上视同 auto。
        // 完全折返的拐点两向量相消，EmitMarker 会因长度为 0 拒画
        for (int i = 1; i + 1 < pts.size(); ++i) {
          arrows += EmitMarker(
              defs.value(id), pts.at(i),
              (pts.at(i) - pts.at(i - 1)) + (pts.at(i + 1) - pts.at(i)), false,
              stroke_width);
        }
      } else if (kind == QLatin1String("end")) {
        arrows += EmitMarker(defs.value(id), pts.last(),
                             pts.last() - pts.at(pts.size() - 2), false,
                             stroke_width);
      }
    }
    cleaned.remove(kRef);
    out += cleaned;

    // 箭头必须是图形的**兄弟**节点：塞进 <line>/<path> 里面 SVG 根本不画
    const QString close_tag =
        QStringLiteral("</") + match.captured(1) + QChar('>');
    int p = pos;
    while (p < svg.size() && svg[p].isSpace()) {
      ++p;
    }
    if (svg.midRef(p, close_tag.size()) == close_tag) {
      out += close_tag;
      pos = p + close_tag.size();
    }
    out += arrows;
  }
  out += svg.mid(pos);

  // 定义删干净：留着会被 QSvgRenderer 当普通节点画到原点
  static const QRegularExpression kMarkerDef(
      QStringLiteral("<marker\\b[^>]*>.*?</marker>"),
      QRegularExpression::DotMatchesEverythingOption);
  out.remove(kMarkerDef);
  return out;
}
