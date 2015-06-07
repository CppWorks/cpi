#include <QtCore>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef Q_OS_WIN32
# include <windows.h>
#endif

#define DEFAULT_CFLAG   "-fPIE -DQT_CORE_LIB "
#define DEFAULT_LDFLAG  "-lpthread "

#define CPI_SRC                                                         \
    "#include <iostream>\n"                                             \
    "#include <typeinfo>\n"                                             \
    "#include <QtCore>\n"                                               \
    "%1\n"                                                              \
    "#include <QStringList>\n"                                          \
    "#include <QChar>\n"                                                \
    "#include <QTextCodec>\n"                                           \
    "#define PRINT_IF(type)  if (ti == typeid(type)) { std::cout << (*(type *)p) << std::endl; }\n" \
    "\n"                                                                \
    "int main() {\n"                                                    \
    "  QTextCodec *codec = QTextCodec::codecForName(\"UTF-8\");\n"      \
    "  QTextCodec::setCodecForLocale(codec);\n"                         \
    "#if QT_VERSION < 0x050000\n"                                       \
    "  QTextCodec::setCodecForTr(codec);\n"                             \
    "  QTextCodec::setCodecForCStrings(codec);\n"                       \
    "#endif\n"                                                          \
    "  auto x_x = ({ %2});\n"                                           \
    "  void *p = (void *)&x_x;\n"                                       \
    "  const std::type_info &ti = typeid(x_x);\n"                       \
    "  if (ti == typeid(char *) || ti == typeid(unsigned char *) || ti == typeid(char const *)) {\n" \
    "    std::cout << '\\\"' << (*(char **)p) << '\\\"'<< std::endl;\n" \
    "  } else if (ti == typeid(std::string)) {\n"                       \
    "    std::cout << '\\\"' << (*(std::string *)p) << '\\\"'<< std::endl;\n" \
    "  } else if (ti == typeid(char)) {\n"                              \
    "    std::cout << (int)(*(char *)p) << std::endl;\n"                \
    "  } else if (ti == typeid(unsigned char)) {\n"                     \
    "    std::cout << (unsigned int)(*(unsigned char *)p) << std::endl;\n" \
    "  } else PRINT_IF(short)\n"                                        \
    "  else PRINT_IF(unsigned short)\n"                                 \
    "  else PRINT_IF(int)\n"                                            \
    "  else PRINT_IF(unsigned int)\n"                                   \
    "  else PRINT_IF(long)\n"                                           \
    "  else PRINT_IF(unsigned long)\n"                                  \
    "  else PRINT_IF(long long)\n"                                      \
    "  else PRINT_IF(unsigned long long)\n"                             \
    "  else PRINT_IF(float)\n"                                          \
    "  else PRINT_IF(double)\n"                                         \
    "  else if (ti == typeid(bool)) {\n"                                \
    "    if (*(bool *)p)\n"                                             \
    "      std::cout << \"true\" << std::endl;\n"                       \
    "    else\n"                                                        \
    "      std::cout << \"false\" << std::endl;\n"                      \
    "  }\n"                                                             \
    "  else PRINT_IF(qint64)\n"                                         \
    "  else PRINT_IF(quint64)\n"                                        \
    "  else if (ti == typeid(QString)) {\n"                             \
    "    std::cout << '\\\"' << qPrintable(*(QString *)p) << '\\\"' << std::endl;\n" \
    "  } else if (ti == typeid(QLatin1String)) {\n"                     \
    "    std::cout << '\\\"' << ((QLatin1String *)p)->latin1() << '\\\"' << std::endl;\n" \
    "  } else if (ti == typeid(QChar)) {\n"                             \
    "    std::cout << '\\'' << qPrintable(QString(*(QChar *)p)) << '\\'' << std::endl;\n" \
    "  } else if (ti == typeid(QStringList) || ti == typeid(QList<QString>)) {\n" \
    "    QStringList list;\n"                                           \
    "    for (QListIterator<QString> i(*(QList<QString> *)p); i.hasNext(); )\n" \
    "      list << QChar('\\\"') + i.next() + '\\\"';\n"                \
    "    std::cout << '[' << qPrintable(list.join(\", \")) << ']' << std::endl;\n" \
    "  }\n"                                                             \
    "  else if (ti == typeid(void *)) {\n"                              \
    "    // print nothing\n"                                            \
    "  } else {\n"                                                      \
    "    // disable to print\n"                                         \
    "    std::cout << \"# disable to print : name:\" << ti.name() << \"  size:\" << sizeof(x_x) << std::endl;\n" \
    "  }\n"                                                             \
    "  return 0;\n"                                                     \
    "}"

#define DEFAULT_CONFIG                                          \
    "[General]\n\n"                                             \
    "### Example option for Qt5\n"                              \
    "#CC=g++\n"                                                 \
    "#CC_FLAGS=-I/usr/include/qt5 -I/usr/include/qt5/QtCore\n"  \
    "#CC_LFLAGS=-lQt5Core\n"                                    \
    "#COMMON_INCLUDES=\n\n"                                     \
    "CC=g++\n"                                                  \
    "CC_FLAGS=-I/usr/include/qt5 -I/usr/include/qt5/QtCore\n"   \
    "CC_LFLAGS=-lQt5Core\n"                                     \
    "COMMON_INCLUDES=\n"


// Entered headers and code
static QStringList headers, code;
static QSettings *conf;


static void showHelp()
{
    char help[] =
        " .conf        Display the current values for various settings.\n" \
        " .help        Display this help.\n"                               \
        " .rm LINENO   Remove the code of the specified line number.\n"    \
        " .show        Show the current source code.\n"                    \
        " .quit        Exit this program.\n";
    printf("%s", help);
}


static void showConfigs(const QSettings &conf)
{
    QStringList confkeys;
    confkeys << "CC" << "CC_FLAGS" << "CC_LFLAGS" << "COMMON_INCLUDES";

    QStringList configs = conf.allKeys();
    for (QStringListIterator it(conf.allKeys()); it.hasNext(); ) {
        const QString &key = it.next();
        if (confkeys.contains(key))
            printf("%s=%s\n", qPrintable(key), qPrintable(conf.value(key).toString()));
    }
}


static void deleteLine(int n)
{
    int h = headers.count();
    int c = code.count();

    if (n > 0) {
        if (n <= h) {
            headers.removeAt(n - 1);
        } else if (n <= h + c) {
            code.removeAt(n - h - 1);
        } else {
            // ignore
        }
    }
}


static void deleteLines(const QList<int> &numbers)
{
    QList<int> list = numbers.toSet().toList(); // removes duplicates
    qSort(list.begin(), list.end(), qGreater<int>());  // sort
    for (QListIterator<int> it(list); it.hasNext(); ) {
        deleteLine(it.next());
    }
}


static void showCode()
{
    int num = 1;
    if (!headers.isEmpty()) {
        for (int i = 0; i < headers.count(); ++i)
            printf("%3d| %s\n", num++, qPrintable(headers.at(i)));
        printf("    --------------------\n");
    }
    for (int i = 0; i < code.count(); ++i)
        printf("%3d| %s\n", num++, qPrintable(code.at(i)));
}


static bool waitForReadyReadStdin(int msec)
{
#ifdef Q_OS_WIN32
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    return (WaitForSingleObject(h, msec) == WAIT_OBJECT_0);
#else
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    struct timeval tv = { 0, 0 };
    tv.tv_usec = 1000 * msec;
    int stdinReady = select(1, &fds, NULL, NULL, &tv); // select for stdin
    if (stdinReady < 0) {
        fprintf(stderr, "select error\n");
    }
    return (stdinReady > 0);
#endif
}


static void printCompileError(const QByteArray &msg)
{
    int idx = msg.indexOf(": ");
    if (idx > 0) {
        QByteArray s = msg.mid(idx + 1);
        printf("%s\n", s.data());
    } else {
        printf("%s\n", msg.data());
    }
}


static bool compile(const QByteArray &cmd, const QByteArray &code, QByteArray& error)
{
    QProcess compile;
    compile.start(cmd);
    compile.write(code);
#if 1
    QFile file("dummy.cpp");
    if (file.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        file.write(qPrintable(code));
        file.close();
    }
#endif
    compile.waitForBytesWritten();
    compile.closeWriteChannel();
    compile.waitForFinished();
    error = compile.readAllStandardError();
    return (compile.exitStatus() == QProcess::NormalExit && compile.exitCode() == 0);
}


static bool compileAndExecute(const QString &src, QByteArray &err)
{
    QByteArray cc = conf->value("CC").toByteArray();
    QByteArray flags = DEFAULT_CFLAG;
    QByteArray lflags= DEFAULT_LDFLAG;
    flags += conf->value("CC_FLAGS").toByteArray();
    lflags += conf->value("CC_LFLAGS").toByteArray();
    QByteArray aout = (QDir::homePath() + QDir::separator()).toLatin1();
#ifdef Q_OS_WIN32
    aout += "cpiout.exe";
#else
    aout += "cpi.out";
#endif

    QByteArray cmd;
    cmd = cc + " -pipe -std=c++0x " + flags + " -xc++ -o ";
    cmd += aout;
    cmd += " - ";  // standard input
    cmd += lflags;
#if 0
    printf("%s\n", cmd.data());
#endif

    bool cpl = compile(cmd, qPrintable(src), err);
    // printf("# %s\n", err.data());
    // printf("----------------\n");

    if (cpl) {
        // Executes the binary
        QProcess exe;
        exe.setProcessChannelMode(QProcess::MergedChannels);
        exe.start(aout);
        exe.waitForFinished();
        printf("%s", exe.readAll().data());
    }

    QFile::remove(aout);
    return cpl;
}


static int compileAndExecuteFile(const char *path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
        return -1;

    QByteArray err;
    QByteArray rawsrc = file.readAll();
    QString src = QString::fromLatin1(rawsrc);

    if (src.mid(0,2) == "#!") {
        // delete first line
        int idx = src.indexOf("\n");
        if (idx > 0)
            src.remove(0, idx);
    }

    QRegExp rx("int\\s+main\\s*\\([^\\(]*\\)");
    if (!src.contains(rx)) {
        QRegExp macro("(#[^\\n]+|\\s*|using\\s+namespace[^\\n]+)\\n");
        int p = 0;
        while (macro.indexIn(src, p) == p) {
            p += macro.matchedLength();
        }
        src.insert(p, "int main(){");
        src += ";return 0;}";
    }
    //printf("---\n%s\n---\n", qPrintable(src));

    bool res = compileAndExecute(src, err);
    if (!res) {
        // print error
        printf("%s\n", err.data());
        return -1;
    }
    return 0;
}


int main(int argv, char *argc[])
{
    QCoreApplication app(argv, argc);

#if (defined Q_OS_WIN32) || (defined Q_OS_DARWIN)
    conf = new QSettings(QSettings::IniFormat, QSettings::UserScope, "cpi/cpi");
#else
    conf = new QSettings(QSettings::NativeFormat, QSettings::UserScope, "cpi/cpi");
#endif

    if (conf->allKeys().isEmpty()) {
        conf->setValue("CC",   "g++");
        conf->setValue("CC_FLAGS",  "");
        conf->setValue("CC_LFLAGS", "");
        conf->setValue("COMMON_INCLUDES", "");
        conf->sync();
    }

    if (argv > 1) {
        return compileAndExecuteFile(argc[1]);
    }

    printf("Loaded INI file: %s\n", qPrintable(conf->fileName()));

    // Mac OS X
    //  cmd = "g++ -pipe -std=c++0x -gdwarf-2 -Wall -W -DQT_CORE_LIB -DQT_SHARED -I/usr/local/Qt4.7/mkspecs/macx-g++ -I. -I/Library/Frameworks/QtCore.framework/Versions/4/Headers -I/usr/include/QtCore -I/usr/include -I. -F/Library/Frameworks -headerpad_max_install_names -F/Library/Frameworks -L/Library/Frameworks -framework QtCore -xc++ -o ";

    // Linux
    // cmd = "g++ -pipe -std=c++0x -D_REENTRANT -DQT_NO_DEBUG -DQT_GUI_LIB -DQT_CORE_LIB -DQT_SHARED -I/usr/share/qt4/mkspecs/linux-g++ -I. -I/usr/include/qt4/QtCore -I/usr/include/qt4 -L/usr/lib -lQtCore -lpthread -xc++ -o ";

    QStringList includes = conf->value("COMMON_INCLUDES").toString().split(" ", QString::SkipEmptyParts);
    for (QStringListIterator i(includes); i.hasNext(); ) {
        QString s = i.next().trimmed();
        if (!s.isEmpty()) {
            if (s.startsWith("<") || s.startsWith('"')) {
                headers << QString("#include ") + s;
            } else {
                headers << QString("#include <") + s + ">";
            }
        }
    }

    bool stdinReady = false;
    for (;;) {
        char line[1024];

        if (!stdinReady) {
            printf("cpi> ");
        }

        char *res = fgets(line, sizeof(line), stdin);

        QString str = QString(line).trimmed();
        if (!res || str == ".quit" || str == ".q")
            break;

        if (str == ".help" || str == "?") {  // shows help
            showHelp();
            continue;
        }

        if (str == ".show" || str == ".code") {  // shows code
            showCode();
            continue;
        }

        if (str == ".conf") {  // shows configs
            showConfigs(*conf);
            continue;
        }

        if (str.startsWith(".del ") || str.startsWith(".rm ")) { // Deletes code
            int n = str.indexOf(' ');
            str.remove(0, n + 1);
            QStringList list = str.split(QRegExp("[,\\s]"), QString::SkipEmptyParts);

            QList<int> numbers; // line-numbers
            for (QStringListIterator it(list); it.hasNext(); ) {
                const QString &s = it.next();
                bool ok;
                int n = s.toInt(&ok);
                if (ok && n > 0)
                    numbers << n;
            }
            deleteLines(numbers);
            showCode();
            continue;
        }

        if (str.startsWith('#') || str.startsWith("using ")) {
            headers << str;
        } else {
            if (!str.isEmpty()) {
                code << str;
            }
        }

        if (code.isEmpty())
            continue;

        stdinReady = waitForReadyReadStdin(10); // wait for 10msec
        if (stdinReady)
            continue;

        QString src = QString(CPI_SRC).arg(headers.join("\n"), code.join("\n"));
        QByteArray err;
        bool cpl = compileAndExecute(src, err);
        if (!cpl) {
            // compile only once more
            src = QString(CPI_SRC).arg(headers.join("\n"), code.join("\n") + "(void *)0;");
            cpl = compileAndExecute(src, err);
        }

        if (!cpl) {
            QString last = code.last();
            if (last.endsWith(';') || last.endsWith('}')) {
#if 1
                // print error
                QList<QByteArray> errs = err.split('\n');
                if (!errs.value(0).contains("int main()")) {
                    printCompileError(errs.value(0));
                } else {
                    printCompileError(errs.value(1));
                }
#else
                printf("%s\n", err.data());
#endif
            }
        }
    }
    return 0;
}
