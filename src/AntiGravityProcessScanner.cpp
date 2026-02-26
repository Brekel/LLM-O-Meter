#include "stdafx.h"
#include "AntiGravityProcessScanner.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define _WIN32_DCOM
#include <windows.h>
#include <wbemidl.h>
#include <iphlpapi.h>
#include <winsock2.h>
#include <string>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "iphlpapi.lib")
#elif defined(Q_OS_MAC)
#include <QProcess>
#endif

#ifdef Q_OS_WIN
AntiGravityServerInfo AntiGravityProcessScanner::scan()
{
	qDebug() << "[AG Scanner] Scanning for process:" << QString::fromWCharArray(ProcessName);

	// Initialize COM for this call (safe to call even if already initialized on this thread)
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	bool callerInitializedCOM = SUCCEEDED(hr); // includes S_FALSE (already init'd)
	qDebug() << "[AG Scanner] CoInitializeEx hr:" << Qt::hex << (uint)hr;

	AntiGravityServerInfo result;

	// Set up COM security (required before connecting to WMI)
	CoInitializeSecurity(
		nullptr,
		-1,
		nullptr,
		nullptr,
		RPC_C_AUTHN_LEVEL_DEFAULT,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		nullptr,
		EOAC_NONE,
		nullptr);

	// Connect to WMI
	IWbemLocator *pLocator = nullptr;
	hr = CoCreateInstance(
		CLSID_WbemLocator,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_IWbemLocator,
		reinterpret_cast<void **>(&pLocator));

	if(FAILED(hr))
	{
		qWarning() << "[AG Scanner] CoCreateInstance(WbemLocator) failed:" << Qt::hex << (uint)hr;
		if(callerInitializedCOM)
			CoUninitialize();
		return result;
	}

	IWbemServices *pServices = nullptr;
	hr = pLocator->ConnectServer(
		const_cast<BSTR>(L"ROOT\\CIMV2"),
		nullptr, nullptr, nullptr, 0, nullptr, nullptr,
		&pServices);

	pLocator->Release();

	if(FAILED(hr))
	{
		qWarning() << "[AG Scanner] ConnectServer(ROOT\\CIMV2) failed:" << Qt::hex << (uint)hr;
		if(callerInitializedCOM)
			CoUninitialize();
		return result;
	}

	CoSetProxyBlanket(
		pServices,
		RPC_C_AUTHN_WINNT,
		RPC_C_AUTHZ_NONE,
		nullptr,
		RPC_C_AUTHN_LEVEL_CALL,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		nullptr,
		EOAC_NONE);

	// Query for the AntiGravity language server process — fetch CommandLine AND ProcessId
	const QString processName = QString::fromWCharArray(ProcessName);
	const QString wql = QString(
		"SELECT CommandLine, ProcessId FROM Win32_Process WHERE Name = '%1'").arg(processName);

	qDebug() << "[AG Scanner] WQL:" << wql;

	IEnumWbemClassObject *pEnumerator = nullptr;
	std::wstring wqlW = wql.toStdWString();
	hr = pServices->ExecQuery(
		const_cast<BSTR>(L"WQL"),
		const_cast<BSTR>(wqlW.c_str()),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		nullptr,
		&pEnumerator);

	qDebug() << "[AG Scanner] ExecQuery hr:" << Qt::hex << (uint)hr;

	if(SUCCEEDED(hr) && pEnumerator)
	{
		IWbemClassObject *pObject = nullptr;
		ULONG returned            = 0;

		while(pEnumerator->Next(WBEM_INFINITE, 1, &pObject, &returned) == S_OK)
		{
			qDebug() << "[AG Scanner] Found process row, reading CommandLine...";
			VARIANT vtCmd;
			VariantInit(&vtCmd);
			VARIANT vtPid;
			VariantInit(&vtPid);

			if(SUCCEEDED(pObject->Get(L"CommandLine", 0, &vtCmd, nullptr, nullptr))
				&& vtCmd.vt == VT_BSTR && vtCmd.bstrVal)
			{
				QString cmdLine = QString::fromWCharArray(vtCmd.bstrVal);
				qDebug() << "[AG Scanner] CommandLine:" << cmdLine.left(300);

				DWORD pid = 0;
				if(SUCCEEDED(pObject->Get(L"ProcessId", 0, &vtPid, nullptr, nullptr))
					&& (vtPid.vt == VT_UI4 || vtPid.vt == VT_I4))
				{
					pid = vtPid.vt == VT_UI4 ? vtPid.uintVal : static_cast<DWORD>(vtPid.intVal);
				}

				AntiGravityServerInfo candidate = parseCommandLine(cmdLine);
				if(candidate.found && pid > 0)
				{
					// Use actual TCP ports the process listens on instead of extensionServerPort+1
					QList<int> ports = getTcpListeningPorts(static_cast<quint32>(pid));
					// Exclude the extension_server_port itself — that's not the API port
					ports.removeAll(candidate.extensionServerPort);
					candidate.candidatePorts = ports;
					qDebug() << "[AG Scanner] PID:" << pid
					         << "| extensionServerPort:" << candidate.extensionServerPort
					         << "| candidatePorts:" << candidate.candidatePorts;
					result = candidate;
					VariantClear(&vtCmd);
					VariantClear(&vtPid);
					pObject->Release();
					break;
				}
				else if(candidate.found)
				{
					// PID unavailable — fall back to extensionServerPort+1 as before
					candidate.candidatePorts = { candidate.extensionServerPort + 1 };
					qDebug() << "[AG Scanner] PID unavailable, falling back to extensionServerPort+1";
					result = candidate;
					VariantClear(&vtCmd);
					VariantClear(&vtPid);
					pObject->Release();
					break;
				}
			}
			else
			{
				qDebug() << "[AG Scanner] CommandLine property missing or empty (vt=" << vtCmd.vt << ")";
			}

			VariantClear(&vtCmd);
			VariantClear(&vtPid);
			pObject->Release();
		}

		pEnumerator->Release();
	}

	pServices->Release();

	if(callerInitializedCOM)
		CoUninitialize();

	return result;
}
#elif defined(Q_OS_MAC)
AntiGravityServerInfo AntiGravityProcessScanner::scan()
{
	qDebug() << "[AG Scanner] Scanning for process:" << ProcessName;

	AntiGravityServerInfo result;

	QProcess ps;
	ps.start("ps", QStringList() << "-eo" << "pid,args");
	if(!ps.waitForFinished(5000))
	{
		qWarning() << "[AG Scanner] ps command timed out";
		return result;
	}

	QString     output = QString::fromUtf8(ps.readAllStandardOutput());
	QStringList lines  = output.split('\n', Qt::SkipEmptyParts);

	for(const QString &line : lines)
	{
		if(!line.contains(QLatin1String(ProcessName)))
			continue;

		QString trimmed  = line.trimmed();
		int     spaceIdx = trimmed.indexOf(' ');
		if(spaceIdx <= 0)
			continue;

		bool    ok  = false;
		quint32 pid = trimmed.left(spaceIdx).toUInt(&ok);
		if(!ok || pid == 0)
			continue;

		QString cmdLine = trimmed.mid(spaceIdx + 1).trimmed();
		qDebug() << "[AG Scanner] Found process — PID:" << pid << "cmdLine:" << cmdLine.left(300);

		AntiGravityServerInfo candidate = parseCommandLine(cmdLine);
		if(candidate.found)
		{
			QList<int> ports = getTcpListeningPorts(pid);
			ports.removeAll(candidate.extensionServerPort);
			candidate.candidatePorts = ports;
			qDebug() << "[AG Scanner] PID:" << pid
			         << "| extensionServerPort:" << candidate.extensionServerPort
			         << "| candidatePorts:" << candidate.candidatePorts;
			result = candidate;
			break;
		}
	}

	return result;
}
#endif

#ifdef Q_OS_WIN
QList<int> AntiGravityProcessScanner::getTcpListeningPorts(quint32 pid)
{
	QList<int> ports;

	// Allocate a buffer for the TCP table; resize if needed
	DWORD bufSize = 65536;
	QByteArray buf(static_cast<int>(bufSize), 0);

	DWORD ret = GetExtendedTcpTable(
		buf.data(),
		&bufSize,
		FALSE,
		AF_INET,
		TCP_TABLE_OWNER_PID_LISTENER,
		0);

	if(ret == ERROR_INSUFFICIENT_BUFFER)
	{
		buf.resize(static_cast<int>(bufSize));
		ret = GetExtendedTcpTable(buf.data(), &bufSize, FALSE, AF_INET,
		                          TCP_TABLE_OWNER_PID_LISTENER, 0);
	}

	if(ret != NO_ERROR)
	{
		qWarning() << "[AG Scanner] GetExtendedTcpTable failed:" << ret;
		return ports;
	}

	const auto *table = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID *>(buf.constData());
	for(DWORD i = 0; i < table->dwNumEntries; ++i)
	{
		const MIB_TCPROW_OWNER_PID &row = table->table[i];
		if(row.dwOwningPid == static_cast<DWORD>(pid))
		{
			// ntohs converts network byte order to host byte order
			int port = static_cast<int>(ntohs(static_cast<u_short>(row.dwLocalPort)));
			ports.append(port);
		}
	}

	qDebug() << "[AG Scanner] getTcpListeningPorts for PID" << pid << ":" << ports;
	return ports;
}
#elif defined(Q_OS_MAC)
QList<int> AntiGravityProcessScanner::getTcpListeningPorts(quint32 pid)
{
	QList<int> ports;

	QProcess lsof;
	lsof.start("lsof", QStringList() << "-iTCP" << "-sTCP:LISTEN" << "-a" << "-p" << QString::number(pid) << "-Fn");
	if(!lsof.waitForFinished(5000))
	{
		qWarning() << "[AG Scanner] lsof command timed out for PID" << pid;
		return ports;
	}

	QString     output = QString::fromUtf8(lsof.readAllStandardOutput());
	QStringList lines  = output.split('\n', Qt::SkipEmptyParts);

	for(const QString &line : lines)
	{
		if(!line.startsWith('n'))
			continue;

		int colonIdx = line.lastIndexOf(':');
		if(colonIdx < 0)
			continue;

		bool ok   = false;
		int  port = line.mid(colonIdx + 1).toInt(&ok);
		if(ok && port > 0 && !ports.contains(port))
			ports.append(port);
	}

	qDebug() << "[AG Scanner] getTcpListeningPorts for PID" << pid << ":" << ports;
	return ports;
}
#endif

AntiGravityServerInfo AntiGravityProcessScanner::parseCommandLine(const QString &cmdLine)
{
	AntiGravityServerInfo info;

	// Helper: extract value for --flag=value or --flag value
	auto extractArg = [&](const QString &flag) -> QString {
		// Try --flag=value first
		QString withEq = flag + "=";
		int idx = cmdLine.indexOf(withEq);
		if(idx != -1)
		{
			int start = idx + withEq.length();
			int end   = cmdLine.indexOf(' ', start);
			if(end == -1) end = cmdLine.length();
			return cmdLine.mid(start, end - start).trimmed();
		}
		// Try --flag value (space-separated)
		QString withSpace = flag + " ";
		idx = cmdLine.indexOf(withSpace);
		if(idx != -1)
		{
			int start = idx + withSpace.length();
			int end   = cmdLine.indexOf(' ', start);
			if(end == -1) end = cmdLine.length();
			return cmdLine.mid(start, end - start).trimmed();
		}
		return {};
	};

	// Extract --extension_server_port
	QString portStr = extractArg("--extension_server_port");
	if(portStr.isEmpty())
		return info;
	bool ok      = false;
	int  rawPort = portStr.toInt(&ok);
	if(!ok || rawPort <= 0)
		return info;

	// Extract --csrf_token
	QString token = extractArg("--csrf_token");
	if(token.isEmpty())
		return info;

	info.found               = true;
	info.extensionServerPort = rawPort;
	// candidatePorts is filled by scan() after PID-based TCP port lookup
	info.csrfToken           = token;
	return info;
}
