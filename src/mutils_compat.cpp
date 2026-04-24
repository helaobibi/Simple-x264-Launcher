///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// MUtils Compatibility Layer for macOS - Implementation
///////////////////////////////////////////////////////////////////////////////

#include "mutils_compat.h"

//Qt
#include <QApplication>
#include <QDate>
#include <QTime>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QSysInfo>
#include <QRegularExpression>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSharedMemory>
#include <QDataStream>
#include <QBuffer>
#include <QThread>
#include <QRandomGenerator>
#include <QStyleFactory>

//POSIX
#include <signal.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <cstdlib>

// ============================================================================
// MUtils::OS
// ============================================================================

MUtils::ArgumentMap MUtils::OS::arguments(void)
{
	ArgumentMap args;
	const QStringList cmdLine = QCoreApplication::arguments();
	
	for(int i = 1; i < cmdLine.count(); i++)
	{
		const QString current = cmdLine[i].trimmed();
		if(current.startsWith("--"))
		{
			const QString key = current.mid(2);
			const int eqPos = key.indexOf('=');
			if(eqPos > 0)
			{
				args.insert(key.left(eqPos).toLower(), key.mid(eqPos + 1));
			}
			else if((i + 1 < cmdLine.count()) && !cmdLine[i+1].startsWith("--"))
			{
				args.insert(key.toLower(), cmdLine[++i].trimmed());
			}
			else
			{
				args.insert(key.toLower(), QString());
			}
		}
		else
		{
			// Positional argument, treat as add-file
			args.insert("add-file", current);
		}
	}
	
	return args;
}

QStringList MUtils::OS::crack_command_line(const QString &command_line)
{
	QStringList tokens;
	QString current;
	bool inQuote = false;

	for(int i = 0; i < command_line.length(); i++)
	{
		const QChar c = command_line[i];
		if(c == QLatin1Char('"'))
		{
			inQuote = !inQuote;
		}
		else if(c.isSpace() && !inQuote)
		{
			if(!current.isEmpty())
			{
				tokens << current;
				current.clear();
			}
		}
		else
		{
			current += c;
		}
	}

	if(!current.isEmpty())
	{
		tokens << current;
	}

	return tokens;
}

QDate MUtils::OS::current_date(void)
{
	return QDate::currentDate();
}

QString MUtils::OS::known_folder(const int &folder_id)
{
	switch(folder_id)
	{
	case FOLDER_PROFILE_USER:
		return QDir::homePath();
	case FOLDER_APPDATA_ROAM:
		return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
	case FOLDER_APPDATA_LOCA:
		return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
	default:
		return QString();
	}
}

bool MUtils::OS::is_hibernation_supported(void)
{
	return false;
}

bool MUtils::OS::shutdown_computer(const QString &message, const unsigned int &timeout, const bool &forceShutdown, const bool &hibernate)
{
	Q_UNUSED(message);
	Q_UNUSED(timeout);
	Q_UNUSED(forceShutdown);
	
	QStringList args;
	args << "-e";
	if(hibernate)
	{
		args << "tell application \"System Events\" to sleep";
	}
	else
	{
		args << "tell application \"System Events\" to shut down";
	}
	return QProcess::startDetached("osascript", args);
}

void MUtils::OS::fatal_exit(const wchar_t *const errorMessage)
{
	const QString msg = QString::fromWCharArray(errorMessage);
	qFatal("FATAL: %s", msg.toUtf8().constData());
	abort();
}

bool MUtils::OS::is_executable_file(const QString &path)
{
	QFileInfo fi(path);
	return fi.exists() && fi.isFile() && fi.isExecutable();
}

bool MUtils::OS::is_library_file(const QString &path)
{
	QFileInfo fi(path);
	if(!fi.exists() || !fi.isFile()) return false;
	const QString suffix = fi.suffix().toLower();
	return (suffix == "dylib" || suffix == "so" || suffix == "framework");
}

void MUtils::OS::suspend_process(const QProcess *proc, const bool suspend)
{
	if(proc && (proc->state() == QProcess::Running))
	{
		qint64 pid = proc->processId();
		if(pid > 0)
		{
			::kill(static_cast<pid_t>(pid), suspend ? SIGSTOP : SIGCONT);
		}
	}
}

void MUtils::OS::change_process_priority(const QProcess *proc, const int priority)
{
	if(proc && (proc->state() == QProcess::Running))
	{
		qint64 pid = proc->processId();
		if(pid > 0)
		{
			int niceValue = 0;
			switch(priority)
			{
			case 0: niceValue = 0; break;   // Normal
			case 1: niceValue = -5; break;  // Above normal (lower nice = higher priority)
			case 2: niceValue = 5; break;   // Below normal
			case 3: niceValue = 10; break;  // Idle
			default: niceValue = 0; break;
			}
			setpriority(PRIO_PROCESS, static_cast<id_t>(pid), niceValue);
		}
	}
}

int MUtils::OS::os_architecture(void)
{
	const QString arch = QSysInfo::currentCpuArchitecture();
	if(arch.contains("arm64", Qt::CaseInsensitive) || arch.contains("aarch64", Qt::CaseInsensitive))
		return ARCH_ARM64;
	return ARCH_X64;
}

void MUtils::OS::system_message_err(const wchar_t *const title, const wchar_t *const text)
{
	qCritical("ERROR [%ls]: %ls", title, text);
}

// ============================================================================
// MUtils::CPUFetaures
// ============================================================================

MUtils::CPUFetaures::cpu_info_t MUtils::CPUFetaures::detect(void)
{
	cpu_info_t cpuInfo;
	cpuInfo.features = FLAG_MMX | FLAG_SSE | FLAG_SSE2; // All modern Macs support these
	cpuInfo.count = QThread::idealThreadCount();
	
	const QString arch = QSysInfo::currentCpuArchitecture();
	cpuInfo.x64 = arch.contains("x86_64", Qt::CaseInsensitive) || arch.contains("arm64", Qt::CaseInsensitive) || arch.contains("aarch64", Qt::CaseInsensitive);
	
#ifdef __aarch64__
	cpuInfo.vendor = "Apple";
	cpuInfo.brand = "Apple Silicon";
#else
	cpuInfo.vendor = "GenuineIntel";
	cpuInfo.brand = "Intel";
	
	// Try to read brand string via sysctl
	char brandStr[256] = {0};
	size_t brandLen = sizeof(brandStr);
	if(sysctlbyname("machdep.cpu.brand_string", brandStr, &brandLen, nullptr, 0) == 0)
	{
		cpuInfo.brand = QString::fromUtf8(brandStr);
	}
#endif
	
	return cpuInfo;
}

// ============================================================================
// MUtils::IPCChannel
// ============================================================================

struct MUtils::IPCChannel::Impl
{
	QString serverName;
	QLocalServer *server;
	QLocalSocket *socket;
	QSharedMemory *sharedMem;
	bool isServer;
};

MUtils::IPCChannel::IPCChannel(const QString &applicationId, const quint32 &appVersionNo, const QString &channelId)
{
	p = new Impl();
	p->serverName = QString("%1_%2_%3").arg(applicationId, QString::number(appVersionNo), channelId);
	p->server = nullptr;
	p->socket = nullptr;
	p->sharedMem = new QSharedMemory(p->serverName);
	p->isServer = false;
}

MUtils::IPCChannel::~IPCChannel(void)
{
	if(p->server) { p->server->close(); delete p->server; }
	if(p->socket) { p->socket->close(); delete p->socket; }
	if(p->sharedMem) { if(p->sharedMem->isAttached()) p->sharedMem->detach(); delete p->sharedMem; }
	delete p;
}

MUtils::IPCChannel* MUtils::IPCChannel::create(const QString &applicationId, const quint32 &appVersionNo, const QString &channelId)
{
	IPCChannel *channel = new IPCChannel(applicationId, appVersionNo, channelId);
	
	// Try to create shared memory (first instance becomes server)
	if(channel->p->sharedMem->create(4096))
	{
		channel->p->isServer = true;
		channel->p->server = new QLocalServer();
		QLocalServer::removeServer(channel->p->serverName);
		channel->p->server->listen(channel->p->serverName);
	}
	else
	{
		channel->p->sharedMem->attach();
	}
	
	return channel;
}

bool MUtils::IPCChannel::send(const quint32 &command, const quint32 &flags, const QStringList &params)
{
	QLocalSocket socket;
	socket.connectToServer(p->serverName);
	if(!socket.waitForConnected(5000))
	{
		return false;
	}
	
	QByteArray data;
	QDataStream stream(&data, QIODevice::WriteOnly);
	stream << command << flags << params;
	
	socket.write(data);
	socket.waitForBytesWritten(5000);
	socket.disconnectFromServer();
	return true;
}

bool MUtils::IPCChannel::read(quint32 &command, quint32 &flags, QStringList &params)
{
	if(!p->server) return false;
	
	if(!p->server->waitForNewConnection(1000))
	{
		command = 0; // NOOP
		flags = 0;
		params.clear();
		return true;
	}
	
	QLocalSocket *client = p->server->nextPendingConnection();
	if(!client) return false;
	
	client->waitForReadyRead(5000);
	QByteArray data = client->readAll();
	client->disconnectFromServer();
	delete client;
	
	QDataStream stream(&data, QIODevice::ReadOnly);
	stream >> command >> flags >> params;
	
	return true;
}

// ============================================================================
// MUtils::GUI
// ============================================================================

void MUtils::GUI::scale_widget(QWidget *widget)
{
	Q_UNUSED(widget);
	// No-op on macOS with Qt 6 (automatic HiDPI scaling)
}

void MUtils::GUI::bring_to_front(QWidget *widget)
{
	if(widget)
	{
		widget->show();
		widget->raise();
		widget->activateWindow();
	}
}

void MUtils::GUI::blink_window(QWidget *widget, unsigned int count, unsigned int delay)
{
	Q_UNUSED(count);
	Q_UNUSED(delay);
	if(widget)
	{
		QApplication::alert(widget, 3000);
	}
}

void MUtils::GUI::enable_close_button(QWidget *window, const bool &bEnable)
{
	Q_UNUSED(bEnable);
	// No-op on macOS: close button is always available
	Q_UNUSED(window);
}

// ============================================================================
// MUtils::Sound
// ============================================================================

void MUtils::Sound::beep(const int &beepType)
{
	Q_UNUSED(beepType);
	QApplication::beep();
}

void MUtils::Sound::play_sound(const QString &name, const bool &bAsync)
{
	Q_UNUSED(name);
	Q_UNUSED(bAsync);
	// No-op on macOS (or could use NSSound)
}

// ============================================================================
// MUtils::Version
// ============================================================================

QDate MUtils::Version::app_build_date(void)
{
	// Parse __DATE__ which is "Mon DD YYYY"
	return QDate::fromString(QString::fromLatin1(__DATE__).simplified(), "MMM d yyyy");
}

QTime MUtils::Version::app_build_time(void)
{
	return QTime::fromString(QString::fromLatin1(__TIME__), "hh:mm:ss");
}

QString MUtils::Version::compiler_version(void)
{
#if defined(__clang__)
	return QString("Clang %1.%2.%3").arg(QString::number(__clang_major__), QString::number(__clang_minor__), QString::number(__clang_patchlevel__));
#elif defined(__GNUC__)
	return QString("GCC %1.%2.%3").arg(QString::number(__GNUC__), QString::number(__GNUC_MINOR__), QString::number(__GNUC_PATCHLEVEL__));
#else
	return QString("Unknown Compiler");
#endif
}

QString MUtils::Version::compiler_arch(void)
{
#if defined(__aarch64__)
	return QString("arm64");
#elif defined(__x86_64__)
	return QString("x86_64");
#else
	return QString("unknown");
#endif
}

// ============================================================================
// MUtils::Registry (stubs - no registry on macOS)
// ============================================================================

bool MUtils::Registry::reg_key_exists(const reg_root_t &root, const QString &path, const reg_scope_t &scope)
{
	Q_UNUSED(root); Q_UNUSED(path); Q_UNUSED(scope);
	return false;
}

bool MUtils::Registry::reg_value_read(const reg_root_t &root, const QString &path, const QString &name, QString &value, const reg_scope_t &scope)
{
	Q_UNUSED(root); Q_UNUSED(path); Q_UNUSED(name); Q_UNUSED(value); Q_UNUSED(scope);
	return false;
}

// ============================================================================
// MUtils utility functions
// ============================================================================

void MUtils::init_process(QProcess &process, const QString &wrkDir, const bool &bReplaceTempDir, const QStringList *const extraPaths, const QHash<QString, QString> *const extraEnv)
{
	Q_UNUSED(bReplaceTempDir);
	
	process.setWorkingDirectory(wrkDir);
	
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	
	if(extraPaths && !extraPaths->isEmpty())
	{
		QString path = env.value("PATH");
		for(const QString &extraPath : *extraPaths)
		{
			if(!extraPath.isEmpty())
			{
				path = extraPath + ":" + path;
			}
		}
		env.insert("PATH", path);
	}
	
	if(extraEnv && !extraEnv->isEmpty())
	{
		for(auto it = extraEnv->constBegin(); it != extraEnv->constEnd(); ++it)
		{
			env.insert(it.key(), it.value());
		}
	}
	
	process.setProcessEnvironment(env);
}

QString MUtils::make_unique_file(const QString &outputDir, const QString &baseName, const QString &extension, const bool &fancy)
{
	Q_UNUSED(fancy);
	
	QString filePath = QString("%1/%2.%3").arg(outputDir, baseName, extension);
	if(!QFileInfo::exists(filePath))
	{
		return filePath;
	}
	
	for(int i = 2; i < 10000; i++)
	{
		filePath = QString("%1/%2 (%3).%4").arg(outputDir, baseName, QString::number(i), extension);
		if(!QFileInfo::exists(filePath))
		{
			return filePath;
		}
	}
	
	return QString();
}

unsigned int MUtils::regexp_parse_uint32(const QRegularExpression &regexp, const QString &text)
{
	QRegularExpressionMatch match = regexp.match(text);
	if(match.hasMatch())
	{
		bool ok = false;
		unsigned int val = match.captured(1).toUInt(&ok);
		if(ok) return val;
	}
	return 0;
}

// ============================================================================
// MUtils::Startup
// ============================================================================

QApplication* MUtils::Startup::create_qt(int &argc, char **argv, const QString &appName, const QString &appAuthor, const QString &appDomain, bool disableErrorDialogs)
{
	Q_UNUSED(disableErrorDialogs);
	
	QApplication *app = new QApplication(argc, argv);
	app->setApplicationName(appName);
	app->setOrganizationName(appAuthor);
	app->setOrganizationDomain(appDomain);
	
	// Use Fusion style on macOS for consistent cross-platform look
	if(QStyleFactory::keys().contains("Fusion", Qt::CaseInsensitive))
	{
		app->setStyle(QStyleFactory::create("Fusion"));
	}
	
	return app;
}

int MUtils::Startup::startup(int &argc, char **argv, main_function_t mainFunction, const char *const appName, const bool &debugFlag)
{
	Q_UNUSED(appName);
	Q_UNUSED(debugFlag);
	return mainFunction(argc, argv);
}
