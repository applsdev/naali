// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "MumbleVoipModule.h"
#include "LinkPlugin.h"
#include "ServerObserver.h"
#include "ConnectionManager.h"
#include "ServerInfo.h"

#include "ModuleManager.h"
#include "EC_OgrePlaceable.h"
#include "ConsoleCommandServiceInterface.h"
#include "EventManager.h"
#include "WorldLogicInterface.h"
#include "Entity.h"
#include "CommunicationsService.h"
#include "LinkPlugin.h"
#include "ServerObserver.h"
#include "Provider.h"
#include "ConnectionManager.h"
#include "MemoryLeakCheck.h"

namespace MumbleVoip
{
    std::string MumbleVoipModule::module_name_ = "MumbleVoipModule";

    MumbleVoipModule::MumbleVoipModule()
        : ModuleInterfaceImpl(module_name_),
          link_plugin_(0),
          time_from_last_update_ms_(0),
          server_observer_(0),
          connection_manager_(0),
          use_camera_position_(false),
          mumble_client_started_(false),
          mumble_use_library_(false)
    {
    }

    MumbleVoipModule::~MumbleVoipModule()
    {
        if (mumble_client_started_ && connection_manager_)
            connection_manager_->KillMumbleClient();
        SAFE_DELETE(link_plugin_);
        SAFE_DELETE(server_observer_);
        SAFE_DELETE(connection_manager_);
    }

    void MumbleVoipModule::Load()
    {
        in_world_voice_provider_ = new InWorldVoice::Provider(framework_);
        connection_manager_ = new ConnectionManager(framework_);
        link_plugin_ = new LinkPlugin();
        server_observer_ = new ServerObserver(framework_);
        connect(server_observer_, SIGNAL(MumbleServerInfoReceived(ServerInfo)), this, SLOT(OnMumbleServerInfoReceived(const ServerInfo &)) );
    }

    void MumbleVoipModule::Unload()
    {
    }

    void MumbleVoipModule::Initialize()
    {
        InitializeConsoleCommands();
    }

    void MumbleVoipModule::PostInitialize()
    {
        event_category_framework_ = framework_->GetEventManager()->QueryEventCategory("Framework");
        if (event_category_framework_ == 0)
            LogError("Unable to find event category for Framework");
    }

    void MumbleVoipModule::Uninitialize()
    {
    }

    void MumbleVoipModule::Update(f64 frametime)
    {
        if (link_plugin_ && link_plugin_->IsRunning())
            UpdateLinkPlugin(frametime);
        
        if (connection_manager_)
        {
            Vector3df position, direction;
            if (GetAvatarPosition(position, direction))
                connection_manager_->SetAudioSourcePosition(position);
            connection_manager_->Update(frametime);
        }
    }

    bool MumbleVoipModule::HandleEvent(event_category_id_t category_id, event_id_t event_id, Foundation::EventDataInterface* data)
    {
        if (server_observer_)
            server_observer_->HandleEvent(category_id, event_id, data);

        if (category_id == event_category_framework_ && event_id == Foundation::PROGRAM_OPTIONS)
        {
            Foundation::ProgramOptionsEvent *po_event = checked_static_cast<Foundation::ProgramOptionsEvent*>(data);
            assert(po_event);
            for(int count = 0; count < po_event->argc; ++count )
            {
                QString arg = QString(po_event->argv[count]);
                if (arg == "--usemumblelibrary")
                    mumble_use_library_ = true;
            }
        }

        return false;
    }

    void MumbleVoipModule::UpdateLinkPlugin(f64 frametime)
    {
        if (!link_plugin_)
            return;

        time_from_last_update_ms_ += 1000*frametime;
        if (time_from_last_update_ms_ < UPDATE_TIME_MS_)
            return;
        time_from_last_update_ms_ = 0;

        Vector3df top_vector = Vector3df::UNIT_Z, position, direction;
        if (GetAvatarPosition(position, direction))
            link_plugin_->SetAvatarPosition(position, direction, top_vector);

        if (use_camera_position_)
            if (GetCameraPosition(position, direction))
                link_plugin_->SetCameraPosition(position, direction, top_vector);
        else
            if (GetAvatarPosition(position, direction))
                link_plugin_->SetCameraPosition(position, direction, top_vector);

        link_plugin_->SendData();
    }

    bool MumbleVoipModule::GetAvatarPosition(Vector3df& position, Vector3df& direction)
    {
        using namespace Foundation;
        boost::shared_ptr<WorldLogicInterface> worldLogic = framework_->GetServiceManager()->GetService<WorldLogicInterface>(Service::ST_WorldLogic).lock();
        if (!worldLogic)
            return false;

        Scene::EntityPtr entity = worldLogic->GetUserAvatarEntity();
        if (!entity)
            return false;

        boost::shared_ptr<OgreRenderer::EC_OgrePlaceable> ogre_placeable = entity->GetComponent<OgreRenderer::EC_OgrePlaceable>();
        if (!ogre_placeable)
            return false;

        Quaternion q = ogre_placeable->GetOrientation();
        position = ogre_placeable->GetPosition(); 
        direction = q*Vector3df::UNIT_Z;

        return true;
    }

    bool MumbleVoipModule::GetCameraPosition(Vector3df& position, Vector3df& direction)
    {
        using namespace Foundation;
        boost::shared_ptr<WorldLogicInterface> worldLogic = framework_->GetServiceManager()->GetService<WorldLogicInterface>(Service::ST_WorldLogic).lock();
        if (!worldLogic)
            return false;

        Scene::EntityPtr camera = worldLogic->GetCameraEntity();
        if (!camera)
            return false;

        boost::shared_ptr<OgreRenderer::EC_OgrePlaceable> ogre_placeable = camera->GetComponent<OgreRenderer::EC_OgrePlaceable>();
        if (!ogre_placeable)
            return false;

        Quaternion q = ogre_placeable->GetOrientation();
        position = ogre_placeable->GetPosition(); 
        direction = q*Vector3df::UNIT_X;
        return true;
    }

    void MumbleVoipModule::InitializeConsoleCommands()
    {
        RegisterConsoleCommand(Console::CreateCommand("mumble link", "Start Mumble link plugin: 'mumble link(user_id, context_id)'",
            Console::Bind(this, &MumbleVoipModule::OnConsoleMumbleLink)));
        RegisterConsoleCommand(Console::CreateCommand("mumble unlink", "Stop Mumble link plugin: 'mumble unlink'",
            Console::Bind(this, &MumbleVoipModule::OnConsoleMumbleUnlink)));
        RegisterConsoleCommand(Console::CreateCommand("mumble start", "Start Mumble client application: 'mumble start(server_url)'",
            Console::Bind(this, &MumbleVoipModule::OnConsoleMumbleStart)));
        RegisterConsoleCommand(Console::CreateCommand("mumble enable vad", "Enable voice activity detector",
            Console::Bind(this, &MumbleVoipModule::OnConsoleEnableVoiceActivityDetector)));
        RegisterConsoleCommand(Console::CreateCommand("mumble disable vad", "Disable voice activity detector",
            Console::Bind(this, &MumbleVoipModule::OnConsoleDisableVoiceActivityDetector)));
    }

    Console::CommandResult MumbleVoipModule::OnConsoleMumbleLink(const StringVector &params)
    {
        if (params.size() != 2)
        {
            return Console::ResultFailure("Wrong number of arguments: usage 'mumble link(id, context)'");
        }
        QString id = params[0].c_str();
        QString context = params[1].c_str();
        
        link_plugin_->SetUserIdentity(id);
        link_plugin_->SetContextId(context);
        link_plugin_->SetApplicationName("Naali viewer");
        link_plugin_->SetApplicationDescription("Naali viewer by realXtend project");
        link_plugin_->Start();

        if (!link_plugin_->IsRunning())
        {
            QString error_message = "Link plugin connection cannot be established. ";
            error_message.append(link_plugin_->GetReason());
            return Console::ResultFailure(error_message.toStdString());
        }

        QString message = QString("Mumbe link plugin started: id=%1 context=%2").arg(id).arg(context);
        return Console::ResultSuccess(message.toStdString());
    }

    Console::CommandResult MumbleVoipModule::OnConsoleMumbleUnlink(const StringVector &params)
    {
        if (params.size() != 0)
        {
            return Console::ResultFailure("Wrong number of arguments: usage 'mumble unlink'");
        }

        if (!link_plugin_->IsRunning())
        {
            return Console::ResultFailure("Mumbe link plugin was not running.");
        }

        link_plugin_->Stop();
        return Console::ResultSuccess("Mumbe link plugin stopped.");
    }

    Console::CommandResult MumbleVoipModule::OnConsoleMumbleStart(const StringVector &params)
    {
        if (params.size() != 1)
        {
            return Console::ResultFailure("Wrong number of arguments: usage 'mumble start(server_url)'");
        }
        QString server_url = params[0].c_str();

        try
        {
            ConnectionManager::StartMumbleClient(server_url);
            return Console::ResultSuccess("Mumbe client started.");
        }
        catch(std::exception &e)
        {
            QString error_message = QString("Cannot start Mumble client: %1").arg(QString(e.what()));
            return Console::ResultFailure(error_message.toStdString());        
        }
    }

    void MumbleVoipModule::OnMumbleServerInfoReceived(const ServerInfo &info)
    {
        // begin: Test service API
        if (framework_ &&  framework_->GetServiceManager())
        {
            boost::shared_ptr<Communications::ServiceInterface> comm = framework_->GetServiceManager()->GetService<Communications::ServiceInterface>(Foundation::Service::ST_Communications).lock();
            if (comm.get())
            {
                comm->Register(*in_world_voice_provider_);
            }
        }
        // end: Test service API

        QUrl murmur_url(QString("mumble://%1/%2").arg(info.server).arg(info.channel)); // setScheme method does not add '//' between scheme and host.
        murmur_url.setUserName(info.user_name);
        murmur_url.setPassword(info.password);
        murmur_url.setQueryItems(QList<QPair<QString,QString> >() << QPair<QString,QString>("version", info.version));

        try
        {
            if (mumble_use_library_)
            {
                connection_manager_->OpenConnection(info);
                LogInfo("Mumble connection established.");
                connection_manager_->SendAudio(true);
                LogDebug("Start sending audio.");
            }
            else
            {
                LogInfo("Starting mumble client.");
                ConnectionManager::StartMumbleClient(murmur_url.toString());

                // it takes some time for a mumble client to setup shared memory for link plugins
                // so we have to wait some time before we can start our link plugin.
                user_id_for_link_plugin_ = info.avatar_id;
                context_id_for_link_plugin_ = info.context_id;
                QTimer::singleShot(2000, this, SLOT(StartLinkPlugin()));
                mumble_client_started_ = true;
            }
        }
        catch(std::exception &e)
        {
            QString messge = QString("Cannot start Mumble client: %1").arg(e.what());
            LogError(messge.toStdString());
            return;
        }
    }

    void MumbleVoipModule::StartLinkPlugin()
    {
        link_plugin_->SetUserIdentity(user_id_for_link_plugin_);
        link_plugin_->SetContextId(context_id_for_link_plugin_);
        link_plugin_->SetApplicationName("Naali viewer");
        link_plugin_->SetApplicationDescription("Naali viewer by realXtend project");
        link_plugin_->Start();

        if (link_plugin_->IsRunning())
        {
            QString message = QString("Mumbe link plugin started: id '%1' context '%2'").arg(user_id_for_link_plugin_).arg(context_id_for_link_plugin_);
            LogInfo(message.toStdString());
        }
        else
        {
            QString error_message = QString("Link plugin connection cannot be established. %1 ").arg(link_plugin_->GetReason());
            LogError(error_message.toStdString());
        }
    }

    Console::CommandResult MumbleVoipModule::OnConsoleEnableVoiceActivityDetector(const StringVector &params)
    {
        connection_manager_->EnableVAD(true);
        QString message = QString("Voice activity detector enabled.");
        return Console::ResultSuccess(message.toStdString());
    }

    Console::CommandResult MumbleVoipModule::OnConsoleDisableVoiceActivityDetector(const StringVector &params)
    {
        connection_manager_->EnableVAD(false);
        QString message = QString("Voice activity detector disabled.");
        return Console::ResultSuccess(message.toStdString());
    }

} // end of namespace: MumbleVoip

extern "C" void POCO_LIBRARY_API SetProfiler(Foundation::Profiler *profiler);
void SetProfiler(Foundation::Profiler *profiler)
{
    Foundation::ProfilerSection::SetProfiler(profiler);
}

using namespace MumbleVoip;

POCO_BEGIN_MANIFEST(Foundation::ModuleInterface)
    POCO_EXPORT_CLASS(MumbleVoipModule)
POCO_END_MANIFEST
