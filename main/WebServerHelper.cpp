#include "stdafx.h"
#include "WebServerHelper.h"
#include "../main/Logger.h"
#include "../main/SQLHelper.h"

namespace http {
	namespace server {
#ifndef NOCLOUD
		extern CProxySharedData sharedData;
#endif

		CWebServerHelper::CWebServerHelper()
		{
			m_pDomServ = nullptr;
		}

		CWebServerHelper::~CWebServerHelper()
		{
			StopServers();
		}
#ifdef WWW_ENABLE_SSL
		bool CWebServerHelper::StartServers(server_settings & web_settings, ssl_server_settings & secure_web_settings, const std::string &serverpath, const bool bIgnoreUsernamePassword, tcp::server::CTCPServer *sharedServer)
#else
		bool CWebServerHelper::StartServers(server_settings & web_settings, const std::string &serverpath, const bool bIgnoreUsernamePassword, tcp::server::CTCPServer *sharedServer)
#endif
		{
			bool bRet = false;

			m_pDomServ = sharedServer;

			our_serverpath = serverpath;
			plainServer_.reset(new CWebServer());
			serverCollection.push_back(plainServer_);
			bRet |= plainServer_->StartServer(web_settings, serverpath, bIgnoreUsernamePassword);
			our_listener_port = web_settings.listening_port;
#ifdef WWW_ENABLE_SSL
			if (secure_web_settings.is_enabled()) {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
				SSL_library_init();
#endif
				secureServer_.reset(new CWebServer());
				bRet |= secureServer_->StartServer(secure_web_settings, serverpath, bIgnoreUsernamePassword);
				serverCollection.push_back(secureServer_);
			}
#endif

#ifndef NOCLOUD
			// start up the mydomoticz proxy client
			RestartProxy();
#endif

			return bRet;
		}

		void CWebServerHelper::StopServers()
		{
			for (auto &it : serverCollection)
			{
				it->StopServer();
			}
			serverCollection.clear();
			plainServer_.reset();
#ifdef WWW_ENABLE_SSL
			secureServer_.reset();
#endif

#ifndef NOCLOUD
			proxymanager.Stop();
#endif
		}

#ifndef NOCLOUD
		void CWebServerHelper::RestartProxy() {
			sharedData.StopTCPClients();
			proxymanager.Stop();
			// restart
#ifdef WWW_ENABLE_SSL
			cWebem *my_pWebEm = (plainServer_ != nullptr && plainServer_->m_pWebEm != nullptr ? plainServer_->m_pWebEm
							     : (secureServer_ != nullptr && secureServer_->m_pWebEm != nullptr ? secureServer_->m_pWebEm : nullptr));
#else
			cWebem* my_pWebEm = plainServer_ != nullptr && plainServer_->m_pWebEm != nullptr ? plainServer_->m_pWebEm : nullptr;
#endif
			if (my_pWebEm == nullptr)
			{
				_log.Log(LOG_ERROR, "No servers are configured. Hence mydomoticz will not be started either (if configured)");
				return;
			}
			if (proxymanager.Start(my_pWebEm, m_pDomServ)) {
				_log.Log(LOG_STATUS, "Proxymanager started.");
			}
		}

		CProxyClient *CWebServerHelper::GetProxyForMaster(DomoticzTCP *master) {
			return proxymanager.GetProxyForMaster(master);
		}

		void CWebServerHelper::RemoveMaster(DomoticzTCP *master) {
			sharedData.RemoveTCPClient(master);
		}
#endif

		void CWebServerHelper::SetWebCompressionMode(const _eWebCompressionMode gzmode)
		{
			for (auto &it : serverCollection)
			{
				it->SetWebCompressionMode(gzmode);
			}
		}

		void CWebServerHelper::SetAuthenticationMethod(const _eAuthenticationMethod amethod)
		{
			for (auto &it : serverCollection)
			{
				it->SetAuthenticationMethod(amethod);
			}
		}

		void CWebServerHelper::SetWebTheme(const std::string &themename)
		{
			for (auto &it : serverCollection)
			{
				it->SetWebTheme(themename);
			}
		}

		void CWebServerHelper::SetWebRoot(const std::string &webRoot)
		{
			for (auto &it : serverCollection)
			{
				it->SetWebRoot(webRoot);
			}
			proxymanager.SetWebRoot(webRoot);
		}

		void CWebServerHelper::LoadUsers()
		{
			for (auto &it : serverCollection)
			{
				it->LoadUsers();
			}
		}
		
		void CWebServerHelper::ClearUserPasswords()
		{
			for (auto &it : serverCollection)
			{
				it->ClearUserPasswords();
			}
		}

		void CWebServerHelper::ReloadLocalNetworksAndProxyIPs()
		{
			std::string WebLocalNetworks, WebRemoteProxyIPs;
			m_sql.GetPreferencesVar("WebLocalNetworks", WebLocalNetworks);
			m_sql.GetPreferencesVar("WebRemoteProxyIPs", WebRemoteProxyIPs);

			for (auto &it : serverCollection)
			{
				it->m_pWebEm->ClearLocalNetworks();

				std::vector<std::string> strarray;
				StringSplit(WebLocalNetworks, ";", strarray);
				for (const auto &str : strarray)
					it->m_pWebEm->AddLocalNetworks(str);
				// add local hostname
				it->m_pWebEm->AddLocalNetworks("");

				it->m_pWebEm->ClearRemoteProxyIPs();
				strarray.clear();
				StringSplit(WebRemoteProxyIPs, ";", strarray);
				for (const auto &str : strarray)
					it->m_pWebEm->AddRemoteProxyIPs(str);
			}
		}

		//JSon
		void CWebServerHelper::	GetJSonDevices(
			Json::Value &root,
			const std::string &rused,
			const std::string &rfilter,
			const std::string &order,
			const std::string &rowid,
			const std::string &planID,
			const std::string &floorID,
			const bool bDisplayHidden,
			const bool bDisplayDisabled,
			const bool bFetchFavorites,
			const time_t LastUpdate,
			const std::string &username,
			const std::string &hardwareid) // OTO
		{
			if (plainServer_) { // assert
				plainServer_->GetJSonDevices(root, rused, rfilter, order, rowid, planID, floorID, bDisplayHidden, bDisplayDisabled, bFetchFavorites, LastUpdate, username, hardwareid);
			}
#ifdef WWW_ENABLE_SSL
			else if (secureServer_) {
				secureServer_->GetJSonDevices(root, rused, rfilter, order, rowid, planID, floorID, bDisplayHidden, bDisplayDisabled, bFetchFavorites, LastUpdate, username, hardwareid);
			}
#endif
		}

		void CWebServerHelper::ReloadCustomSwitchIcons()
		{
			for (auto &it : serverCollection)
			{
				it->ReloadCustomSwitchIcons();
			}
		}
	} // namespace server

} // namespace http
