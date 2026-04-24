///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2024 LoRd_MuldeR <MuldeR2@GMX.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// http://www.gnu.org/licenses/gpl-2.0.txt
///////////////////////////////////////////////////////////////////////////////

#include "encoder_x265.h"

//Internal
#include "global.h"
#include "model_options.h"
#include "model_status.h"
#include "mediainfo.h"
#include "model_sysinfo.h"
#include "model_clipInfo.h"

//MUtils compat
#include "mutils_compat.h"

//Qt
#include <QStringList>
#include <QDir>
#include <QRegularExpression>
#include <QPair>

//x265 version info
static const unsigned int VERSION_X265_MINIMUM_VER = 36;
static const unsigned int VERSION_X265_MINIMUM_REV =  0;

// ------------------------------------------------------------
// Helper Macros
// ------------------------------------------------------------

#define X265_UPDATE_PROGRESS(M) do \
{ \
	bool ok[2] = { false, false }; \
	unsigned int progressInt = (M).captured(1).toUInt(&ok[0]); \
	unsigned int progressFrc = (M).captured(2).toUInt(&ok[1]); \
	setStatus((pass == 2) ? JobStatus_Running_Pass2 : ((pass == 1) ? JobStatus_Running_Pass1 : JobStatus_Running)); \
	if(ok[0] && ok[1]) \
	{ \
		const double progress = (double(progressInt) / 100.0) + (double(progressFrc) / 1000.0); \
		if(!qFuzzyCompare(progress, last_progress)) \
		{ \
			setProgress(floor(progress * 100.0)); \
			size_estimate = qFuzzyIsNull(size_estimate) ? estimateSize(m_outputFile, progress) : ((0.667 * size_estimate) + (0.333 * estimateSize(m_outputFile, progress))); \
			last_progress = progress; \
		} \
	} \
	setDetails(tr("%1, est. file size %2").arg(line.mid(offset).trimmed(), sizeToString(qRound64(size_estimate)))); \
} \
while(0)

#define REMOVE_CUSTOM_ARG(LIST, ITER, FLAG, PARAM) do \
{ \
	if(ITER != LIST.end()) \
	{ \
		if((*ITER).compare(PARAM, Qt::CaseInsensitive) == 0) \
		{ \
			log(tr("WARNING: Custom parameter \"" PARAM "\" will be ignored in Pipe'd mode!\n")); \
			ITER = LIST.erase(ITER); \
			if(ITER != LIST.end()) \
			{ \
				if(!((*ITER).startsWith("--", Qt::CaseInsensitive))) ITER = LIST.erase(ITER); \
			} \
			FLAG = true; \
		} \
	} \
} \
while(0)

// ------------------------------------------------------------
// Encoder Info
// ------------------------------------------------------------

class X265EncoderInfo : public AbstractEncoderInfo
{
public:
	virtual QString getName(void) const
	{
		return "x265 (HEVC/H.265)";
	}

	virtual QList<ArchId> getArchitectures(void) const
	{
		return QList<ArchId>()
		<< qMakePair(QString("Native"), ARCH_TYPE_X64);
	}

	virtual QStringList getVariants(void) const
	{
		return QStringList() << "8-Bit" << "10-Bit" << "12-Bit";
	}
	virtual QList<RCMode> getRCModes(void) const
	{
		return QList<RCMode>()
		<< qMakePair(QString("CRF"),    RC_TYPE_QUANTIZER)
		<< qMakePair(QString("CQ"),     RC_TYPE_QUANTIZER)
		<< qMakePair(QString("2-Pass"), RC_TYPE_MULTIPASS)
		<< qMakePair(QString("ABR"),    RC_TYPE_RATE_KBPS);
	}

	virtual QStringList getTunings(void) const
	{
		return QStringList() << "Grain" << "PSNR" << "SSIM" << "FastDecode" << "ZeroLatency" << "Animation";
	}

	virtual QStringList getPresets(void) const
	{
		return QStringList()
		<< "ultrafast" << "superfast" << "veryfast" << "faster"   << "fast"
		<< "medium"    << "slow"      << "slower"   << "veryslow" << "placebo";
	}

	virtual QStringList getProfiles(const quint32 &variant) const
	{
		QStringList profiles;
		switch(variant)
		{
			case 0: profiles << "main"   << "main-intra"   << "mainstillpicture" << "main444-8"        << "main444-intra" << "main444-stillpicture"; break;
			case 1: profiles << "main10" << "main10-intra" << "main422-10"       << "main422-10-intra" << "main444-10"    << "main444-10-intra";     break;
			case 2: profiles << "main12" << "main12-intra" << "main422-12"       << "main422-12-intra" << "main444-12"    << "main444-12-intra";     break;
			default: MUTILS_THROW("Unknown encoder variant!");
		}
		return profiles;
	}

	virtual QStringList supportedOutputFormats(void) const
	{
		return QStringList() << "hevc";
	}

	virtual bool isInputTypeSupported(const int format) const
	{
		switch(format)
		{
		case MediaInfo::FILETYPE_VAPOURSYNTH:
		case MediaInfo::FILETYPE_YUV4MPEG2:
			return true;
		default:
			return false;
		}
	}

	virtual QString getBinaryPath(const SysinfoModel *sysinfo, const quint32 &encArch, const quint32 &encVariant) const
	{
		Q_UNUSED(sysinfo);
		Q_UNUSED(encArch);
		Q_UNUSED(encVariant);
		return QString("/opt/homebrew/bin/x265");
	}

	virtual QString getHelpCommand(void) const
	{
		return "--fullhelp";
	}
};

static const X265EncoderInfo s_x265EncoderInfo;

const AbstractEncoderInfo& X265Encoder::encoderInfo(void)
{
	return s_x265EncoderInfo;
}

const AbstractEncoderInfo &X265Encoder::getEncoderInfo(void) const
{
	return encoderInfo();
}

// ------------------------------------------------------------
// Constructor & Destructor
// ------------------------------------------------------------

X265Encoder::X265Encoder(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile, const QString &outputFile)
:
	AbstractEncoder(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause, sourceFile, outputFile)
{
	if(options->encType() != OptionsModel::EncType_X265)
	{
		MUTILS_THROW("Invalid encoder type!");
	}
}

X265Encoder::~X265Encoder(void)
{
	/*Nothing to do here*/
}

QString X265Encoder::getName(void) const
{
	return s_x265EncoderInfo.getFullName(m_options->encArch(), m_options->encVariant());
}

// ------------------------------------------------------------
// Check Version
// ------------------------------------------------------------

void X265Encoder::checkVersion_init(QList<QRegularExpression*> &patterns, QStringList &cmdLine)
{
	cmdLine << "--version";
	patterns << new QRegularExpression("\\bHEVC\\s+encoder\\s+version\\s+(\\d)\\.(\\d+)\\+(\\d+)\\b",      QRegularExpression::CaseInsensitiveOption);
	patterns << new QRegularExpression("\\bHEVC\\s+encoder\\s+version\\s+(\\d)\\.(\\d+)\\_Au\\+(\\d+)\\b", QRegularExpression::CaseInsensitiveOption);
	patterns << new QRegularExpression("\\bHEVC\\s+encoder\\s+version\\s+(\\d)\\.(\\d+)\\b",               QRegularExpression::CaseInsensitiveOption);
}

void X265Encoder::checkVersion_parseLine(const QString &line, const QList<QRegularExpression*> &patterns, unsigned int &core, unsigned int &build, bool &modified)
{
	if(!line.isEmpty())
	{
		log(tr("[x265-stdout] %1").arg(line));
	}

	QRegularExpressionMatch match;
	bool matched = false;

	for (size_t q = 0; q < 2; ++q)
	{
		match = patterns[q]->match(line);
		if (match.hasMatch())
		{
			log(tr("[x265-debug] Pattern[%1] matched: \"%2\"").arg(QString::number(q), match.captured(0)));
			bool ok0 = false, ok1 = false, ok2 = false;
			unsigned int t0 = match.captured(1).toUInt(&ok0);
			unsigned int t1 = match.captured(2).toUInt(&ok1);
			unsigned int t2 = match.captured(3).toUInt(&ok2);
			log(tr("[x265-debug] Captures: major=%1(ok=%2), minor=%3(ok=%4), build=%5(ok=%6)").arg(
				match.captured(1), ok0 ? "y" : "n", match.captured(2), ok1 ? "y" : "n", match.captured(3), ok2 ? "y" : "n"));
			if (ok0 && ok1 && ok2)
			{
				core = (10 * t0) + t1;
				build = t2;
				matched = true;
				break;
			}
		}
	}

	if ((!matched) && ((!core) || (core == UINT_MAX)))
	{
		match = patterns[2]->match(line);
		if (match.hasMatch())
		{
			log(tr("[x265-debug] Pattern[2] (fallback) matched: \"%1\"").arg(match.captured(0)));
			bool ok0 = false, ok1 = false;
			unsigned int t0 = match.captured(1).toUInt(&ok0);
			unsigned int t1 = match.captured(2).toUInt(&ok1);
			log(tr("[x265-debug] Captures: major=%1(ok=%2), minor=%3(ok=%4)").arg(
				match.captured(1), ok0 ? "y" : "n", match.captured(2), ok1 ? "y" : "n"));
			if (ok0 && ok1)
			{
				core = (10 * t0) + t1;
			}
			build = 0;
		}
		else if(!line.isEmpty())
		{
			log(tr("[x265-debug] No pattern matched this line."));
		}
	}
}

QString X265Encoder::printVersion(const unsigned int &revision, const bool &modified)
{
	unsigned int core, build;
	splitRevision(revision, core, build);

	return tr("x265 version: %1.%2+%3").arg(QString::number(core / 10), QString::number(core % 10), QString::number(build));
}

bool X265Encoder::isVersionSupported(const unsigned int &revision, const bool &modified)
{
	unsigned int core, build;
	splitRevision(revision, core, build);

	if((core < VERSION_X265_MINIMUM_VER) || ((core == VERSION_X265_MINIMUM_VER) && (build < VERSION_X265_MINIMUM_REV)))
	{
		log(tr("\nERROR: Your version of x265 is too old! (Minimum required revision is %1.%2+%3)").arg(QString::number(VERSION_X265_MINIMUM_VER / 10), QString::number(VERSION_X265_MINIMUM_VER % 10), QString::number(VERSION_X265_MINIMUM_REV)));
		return false;
	}
	else if(core > VERSION_X265_MINIMUM_VER)
	{
		log(tr("\nWARNING: Your version of x265 is newer than the latest tested version, take care!"));
		log(tr("This application works best with x265 version %1.%2. Newer versions may work or not.").arg(QString::number(VERSION_X265_MINIMUM_VER / 10), QString::number(VERSION_X265_MINIMUM_VER % 10)));
	}
	
	return true;
}

// ------------------------------------------------------------
// Encoding Functions
// ------------------------------------------------------------

void X265Encoder::buildCommandLine(QStringList &cmdLine, const bool &usePipe, const ClipInfo &clipInfo, const QString &indexFile, const int &pass, const QString &passLogFile)
{
	double crf_int = 0.0, crf_frc = 0.0;

	cmdLine << "-D";
	switch (m_options->encVariant())
	{
	case 0:
		cmdLine << QString::number(8);
		break;
	case 1:
		cmdLine << QString::number(10);
		break;
	case 2:
		cmdLine << QString::number(12);
		break;
	default:
		MUTILS_THROW("Unknown encoder variant!");
	}

	switch(m_options->rcMode())
	{
	case 0:
		crf_frc = modf(m_options->quantizer(), &crf_int);
		cmdLine << "--crf" << QString("%1.%2").arg(QString::number(qRound(crf_int)), QString::number(qRound(crf_frc * 10.0)));
		break;
	case 1:
		cmdLine << "--qp" << QString::number(qRound(m_options->quantizer()));
		break;
	case 2:
	case 3:
		cmdLine << "--bitrate" << QString::number(m_options->bitrate());
		break;
	default:
		MUTILS_THROW("Bad rate-control mode !!!");
		break;
	}
	
	if((pass == 1) || (pass == 2))
	{
		cmdLine << "--pass" << QString::number(pass);
		cmdLine << "--stats" << QDir::toNativeSeparators(passLogFile);
	}

	const QString preset = m_options->preset().simplified().toLower();
	if(!preset.isEmpty())
	{
		if(preset.compare(QString::fromLatin1(OptionsModel::SETTING_UNSPECIFIED), Qt::CaseInsensitive) != 0)
		{
			cmdLine << "--preset" << preset;
		}
	}

	const QString tune = m_options->tune().simplified().toLower();
	if(!tune.isEmpty())
	{
		if(tune.compare(QString::fromLatin1(OptionsModel::SETTING_UNSPECIFIED), Qt::CaseInsensitive) != 0)
		{
			cmdLine << "--tune" << tune;
		}
	}

	const QString profile = m_options->profile().simplified().toLower();
	if(!profile.isEmpty())
	{
		if(profile.compare(QString::fromLatin1(OptionsModel::PROFILE_UNRESTRICTED), Qt::CaseInsensitive) != 0)
		{
			cmdLine << "--profile" << profile;
		}
	}

	if(!m_options->customEncParams().isEmpty())
	{
		QStringList customArgs = splitParams(m_options->customEncParams(), m_sourceFile, m_outputFile);
		if(usePipe)
		{
			QStringList::iterator i = customArgs.begin();
			while(i != customArgs.end())
			{
				bool bModified = false;
				REMOVE_CUSTOM_ARG(customArgs, i, bModified, "--fps");
				REMOVE_CUSTOM_ARG(customArgs, i, bModified, "--frames");
				if(!bModified) i++;
			}
		}
		cmdLine.append(customArgs);
	}

	cmdLine << "--output" << QDir::toNativeSeparators(m_outputFile);
	
	if(usePipe)
	{
		if (clipInfo.getFrameCount() < 1)
		{
			MUTILS_THROW("Frames not set!");
		}
		cmdLine << "--frames" << QString::number(clipInfo.getFrameCount());
		cmdLine << "--y4m" << "-";
	}
	else
	{
		cmdLine << QDir::toNativeSeparators(m_sourceFile);
	}
}

void X265Encoder::runEncodingPass_init(QList<QRegularExpression*> &patterns)
{
	patterns << new QRegularExpression("\\[(\\d+)\\.(\\d+)%\\].+frames");   //regExpProgress
	patterns << new QRegularExpression("indexing.+\\[(\\d+)\\.(\\d+)%\\]"); //regExpIndexing
	patterns << new QRegularExpression("^(\\d+) frames:"); //regExpFrameCnt
	patterns << new QRegularExpression("\\[\\s*(\\d+)\\.(\\d+)%\\]\\s+(\\d+)/(\\d+)\\s(\\d+).(\\d+)\\s(\\d+).(\\d+)\\s+(\\d+):(\\d+):(\\d+)\\s+(\\d+):(\\d+):(\\d+)"); //regExpModified
}

void X265Encoder::runEncodingPass_parseLine(const QString &line, const QList<QRegularExpression*> &patterns, const ClipInfo &clipInfo, const int &pass, double &last_progress, double &size_estimate)
{
	int offset = -1;
	QRegularExpressionMatch match;
	if((match = patterns[0]->match(line)).hasMatch())
	{
		offset = match.capturedStart();
		X265_UPDATE_PROGRESS(match);
	}
	else if((match = patterns[1]->match(line)).hasMatch())
	{
		offset = match.capturedStart();
		bool ok = false;
		unsigned int progress = match.captured(1).toUInt(&ok);
		setStatus(JobStatus_Indexing);
		if(ok)
		{
			setProgress(progress);
		}
		setDetails(line.mid(offset).trimmed());
	}
	else if((match = patterns[2]->match(line)).hasMatch())
	{
		offset = match.capturedStart();
		setStatus((pass == 2) ? JobStatus_Running_Pass2 : ((pass == 1) ? JobStatus_Running_Pass1 : JobStatus_Running));
		setDetails(line.mid(offset).trimmed());
	}
	else if((match = patterns[3]->match(line)).hasMatch())
	{
		offset = match.capturedStart();
		X265_UPDATE_PROGRESS(match);
	}
	else if(!line.isEmpty())
	{
		log(line);
	}
}
