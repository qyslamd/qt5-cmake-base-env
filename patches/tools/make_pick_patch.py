"""Generate patches/funq/0008 (route pick output through Qt logging).

用法——工作目录随意，脚本按绝对路径干活：

    python patches/tools/make_pick_patch.py

工作目录的准备（需要一个停在「已打 0007、未打 0008」状态的 funq 副本）：

    mkdir -p temp/patchgen-funq-pick
    tar cf - --exclude=__pycache__ -C 3rdparty/funq-master . \
        | tar xf - -C temp/patchgen-funq-pick
    cd temp/patchgen-funq-pick && git init -q && git add -A && git commit -qm base
    git apply -R ../../patches/funq/0008-pick-output-through-qt-logging.patch

`git apply -R` 这一步是取巧也是稳的：0008 的内容就是 pick.cpp 那一处改动，
反向应用即回到它之前的状态。脚本改完不提交，重跑前 `git checkout .` 复位。

用 tar 而不是 `cp -r` 是为了绕开 `__pycache__`：Windows 上 Defender 隔离过的
`.pyc` 能列出、读不了，复制会直接报 Permission denied。

**注意脚本只插入一次 `#include <QDebug>`**（以 `#include <QGraphicsItem>` 为锚，
替换即止）。对着同一份没复位的树连跑两次会插出两行——已提交的 0008 里就带着
这个痕迹，外加一行不再需要的 `#include <QStringList>`，都是那次工作目录带着
手工改动时生成的。两处都无害（有 include guard，且未使用的 include 不报错），
但没有必要重现。

Kept as a file (not a heredoc) because the C++ text carries \n / \t escapes that
must reach the generated source verbatim.
"""

import pathlib
import re
import subprocess

REPO = pathlib.Path(__file__).resolve().parents[2]
PATCHGEN = REPO / "temp/patchgen-funq-pick"

NEW_FN = r'''void PickFormatter::handle(QObject * object, const QPoint & pos) {
    // Compose the whole block first, then hand it to two sinks: stdout (the
    // upstream behaviour, for terminal users) and the Qt message system.
    //
    // The Qt path is the one hosts can actually collect: this repository's log
    // file and in-app log window are both fed by a qInstallMessageHandler hook.
    // stdout, on the other hand, can go nowhere at all - the tool is injected
    // into a GUI process that may have been started over a pipe chain, and a
    // GUI process started that way gets no usable standard handles, so the
    // writes are dropped silently (measured: after a pick click neither the
    // console nor a redirected file received a single byte).
    QString block;
    {
        QTextStream out(&block);
        out << QString("WIDGET: `%1` (pos: %2, %3)")
                   .arg(ObjectPath::objectPath(object))
                   .arg(pos.x())
                   .arg(pos.y())
            << '\n';
        if (m_showProperties) {
            print_object_props(object, out);
        }

        QGraphicsView * view = dynamic_cast<QGraphicsView *>(object->parent());
        if (view) {
            QGraphicsItem * item = view->itemAt(pos);
            QObject * qitem = dynamic_cast<QObject *>(item);
            if (item) {
                out << "GITEM: `" << ObjectPath::graphicsItemId(item)
                    << "` (QObject: " << (qitem != 0) << ")" << '\n';
                if (qitem) {
                    print_object_props(qitem, out);
                }
            }
        }
    }

    m_stream << block;
    m_stream.flush();

    // One record per click, line breaks kept: the log window then renders a
    // single date/level prefix followed by the whole block (its panel style
    // carries white-space:pre-wrap for exactly this). One record per line
    // would read as N unrelated events instead.
    QString record = block;
    while (record.endsWith(QLatin1Char('\n'))) {
        record.chop(1);
    }
    qInfo().noquote() << record;
}
'''


def main():
    pick = PATCHGEN / "server/libFunq/pick.cpp"
    text = pick.read_text(encoding="utf-8")

    pat = re.compile(
        r"void PickFormatter::handle\(QObject \* object, const QPoint & pos\) \{.*?\n\}\n",
        re.DOTALL,
    )
    assert len(pat.findall(text)) == 1, "handle() anchor not unique"
    text = pat.sub(lambda _m: NEW_FN, text, count=1)

    assert text.count("#include <QGraphicsItem>") == 1
    text = text.replace("#include <QGraphicsItem>",
                        "#include <QDebug>\n#include <QGraphicsItem>")

    with open(pick, "w", encoding="utf-8", newline="") as fh:
        fh.write(text)

    diff = subprocess.run(["git", "diff"], cwd=PATCHGEN,
                          capture_output=True, check=True).stdout
    assert diff, "empty diff"
    out = REPO / "patches/funq/0008-pick-output-through-qt-logging.patch"
    out.write_bytes(diff.replace(b"\r\n", b"\n"))
    print("wrote", out.name, len(diff), "bytes")

    body = out.read_text(encoding="utf-8")
    assert "qInfo().noquote() << record;" in body, "tail not updated"
    assert "parts.join" not in body, "old join-per-line tail survived"
    print("self-check ok")


if __name__ == "__main__":
    main()
