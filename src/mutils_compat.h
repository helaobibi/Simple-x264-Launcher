///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// MUtils Compatibility Layer for macOS
// Replaces the Windows-specific MUtils library with Qt/POSIX equivalents
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <QString>
#include <QRegularExpression>
#include <QStringList>
#include <QDate>
#include <QTime>
#include <QMultiMap>
#include <QWidget>
#include <QProcess>
#include <QApplication>
#include <QHash>
#include <QFileInfo>
#include <stdexcept>

// ============================================================================
// Macros replacing MUtils macros
// ============================================================================

#define MUTILS_DELETE(PTR) do { delete (PTR); (PTR) = nullptr; } while(0)
#define MUTILS_UTF8(STR) ((STR).toUtf8().constData())
#define MUTILS_THROW(MSG) throw std::runtime_error((MSG))
#define MUTILS_BOOL2STR(X) ((X) ? "YES" : "NO")

#ifdef NDEBUG
#define MUTILS_DEBUG 0
#else
#define MUTILS_DEBUG 1
#endif

// ============================================================================
// MUtils::OS namespace
// ============================================================================

namespace MUtils
{
	typedef QMultiMap<QString, QString> ArgumentMap;

	namespace OS
	{
		enum KnownFolder
		{
			FOLDER_PROFILE_USER = 0,
			FOLDER_APPDATA_ROAM = 1,
			FOLDER_APPDATA_LOCA = 2,
		};

		enum Architecture
		{
			ARCH_X64 = 0,
			ARCH_ARM64 = 1,
		};

		ArgumentMap arguments(void);
		QStringList crack_command_line(const QString &command_line);
		QDate current_date(void);
		QString known_folder(const int &folder_id);
		bool is_hibernation_supported(void);
		bool shutdown_computer(const QString &message, const unsigned int &timeout, const bool &forceShutdown, const bool &hibernate);
		void fatal_exit(const wchar_t *const errorMessage);
		bool is_executable_file(const QString &path);
		bool is_library_file(const QString &path);
		void suspend_process(const QProcess *proc, const bool suspend);
		void change_process_priority(const QProcess *proc, const int priority);
		int os_architecture(void);
		void system_message_err(const wchar_t *const title, const wchar_t *const text);
	}

	// ========================================================================
	// MUtils::CPUFetaures namespace (note: original typo preserved)
	// ========================================================================

	namespace CPUFetaures
	{
		enum CPUFlags
		{
			FLAG_MMX  = 0x01,
			FLAG_SSE  = 0x02,
			FLAG_SSE2 = 0x04,
		};

		typedef struct _cpu_info_t
		{
			quint32 features;
			bool x64;
			quint32 count;
			QString vendor;
			QString brand;
		}
		cpu_info_t;

		cpu_info_t detect(void);
	}

	// ========================================================================
	// MUtils::IPCChannel class
	// ========================================================================

	class IPCChannel
	{
	public:
		IPCChannel(const QString &applicationId, const quint32 &appVersionNo, const QString &channelId);
		~IPCChannel(void);

		static IPCChannel *create(const QString &applicationId, const quint32 &appVersionNo, const QString &channelId);

		bool send(const quint32 &command, const quint32 &flags, const QStringList &params);
		bool read(quint32 &command, quint32 &flags, QStringList &params);

	private:
		struct Impl;
		Impl *p;
	};

	// ========================================================================
	// MUtils::Taskbar7 class (stub for macOS)
	// ========================================================================

	class Taskbar7
	{
	public:
		enum TaskbarState
		{
			TASKBAR_STATE_NONE = 0,
			TASKBAR_STATE_NORMAL = 1,
			TASKBAR_STATE_INTERMEDIATE = 2,
			TASKBAR_STATE_ERROR = 3,
			TASKBAR_STATE_PAUSED = 4,
		};

		Taskbar7(QWidget *window) { Q_UNUSED(window); }
		~Taskbar7(void) {}

		void setTaskbarState(const TaskbarState &state) { Q_UNUSED(state); }
		void setTaskbarProgress(const quint64 &currentValue, const quint64 &maximumValue) { Q_UNUSED(currentValue); Q_UNUSED(maximumValue); }
		void setOverlayIcon(const QIcon *icon) { Q_UNUSED(icon); }
	};

	// ========================================================================
	// MUtils::GUI namespace
	// ========================================================================

	namespace GUI
	{
		void scale_widget(QWidget *widget);
		void bring_to_front(QWidget *widget);
		void blink_window(QWidget *widget, unsigned int count = 5, unsigned int delay = 125);
		void enable_close_button(QWidget *window, const bool &bEnable);
	}

	// ========================================================================
	// MUtils::Sound namespace
	// ========================================================================

	namespace Sound
	{
		enum SoundType
		{
			BEEP_NFO = 0,
			BEEP_WRN = 1,
			BEEP_ERR = 2,
		};

		void beep(const int &beepType);
		void play_sound(const QString &name, const bool &bAsync);
	}

	// ========================================================================
	// MUtils::Version namespace
	// ========================================================================

	namespace Version
	{
		QDate app_build_date(void);
		QTime app_build_time(void);
		QString compiler_version(void);
		QString compiler_arch(void);
	}

	// ========================================================================
	// MUtils::Registry namespace (stub)
	// ========================================================================

	namespace Registry
	{
		enum reg_root_t
		{
			root_machine = 0,
			root_user = 1,
		};

		enum reg_scope_t
		{
			scope_default = 0,
			scope_wow_x32 = 1,
			scope_wow_x64 = 2,
		};

		bool reg_key_exists(const reg_root_t &root, const QString &path, const reg_scope_t &scope = scope_default);
		bool reg_value_read(const reg_root_t &root, const QString &path, const QString &name, QString &value, const reg_scope_t &scope = scope_default);
	}

	// ========================================================================
	// Utility functions
	// ========================================================================

	void init_process(QProcess &process, const QString &wrkDir, const bool &bReplaceTempDir = true, const QStringList *const extraPaths = nullptr, const QHash<QString, QString> *const extraEnv = nullptr);
	QString make_unique_file(const QString &outputDir, const QString &baseName, const QString &extension, const bool &fancy = false);
	unsigned int regexp_parse_uint32(const QRegularExpression &regexp, const QString &text);

	// ========================================================================
	// Startup namespace
	// ========================================================================

	namespace Startup
	{
		typedef int (*main_function_t)(int &argc, char **argv);
		QApplication* create_qt(int &argc, char **argv, const QString &appName, const QString &appAuthor, const QString &appDomain, bool disableErrorDialogs = false);
		int startup(int &argc, char **argv, main_function_t mainFunction, const char *const appName, const bool &debugFlag);
	}
}
