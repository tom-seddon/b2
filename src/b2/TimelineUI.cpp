#include <shared/system.h>
#include "TimelineUI.h"

#if TIMELINE_UI_ENABLED

#include "dear_imgui.h"
#include <vector>
#include <SDL.h>
#include <map>
#include "misc.h"
#include "GenerateThumbnailJob.h"
#include "BeebState.h"
#include <shared/debug.h>
#include "BeebWindows.h"
#include "BeebThread.h"
#include <beeb/DiscImage.h>
#include "BeebWindow.h"
#include "b2.h"
#include <inttypes.h>
#include <IconsFontAwesome.h>
#include <algorithm>
#include "native_ui.h"
#include "VideoWriter.h"
#include "Timeline.h"
#include <float.h>
#include "SettingsUI.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "TimelineUI_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "TimelineUI_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string RECENT_PATHS_VIDEO("video");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const ImColor THIS_TIMELINE_COLOUR=ImColor::HSV(.3f,.6f,.7f);
static const ImColor OTHER_TIMELINE_COLOUR=ImColor::HSV(.15f,.3f,.3f);

static const ImVec2 PADDING(8.f,8.f);
static const ImColor TIMELINE_ARROW_COLOUR(.9f,.3f,.2f);
static const float TIMELINE_ARROW_THICKNESS=3.f;
static const ImColor ROUTE_HIGHLIGHT_COLOUR(65.f/360.f,1.f,1.f);
static const float TIMELINE_ARROW_WIDTH=10.f;
static const float TIMELINE_ARROW_HEIGHT=5.f;
static const ImVec2 BOX_SIZE(320.f,350.f);
static const ImVec2 GAP_SIZE(100.f,50.f);
static const ImVec2 THUMBNAIL_SIZE(256.f,220.f); // Should really
                                                 // calculate this
                                                 // value...

// The layout is on a grid.
static const ImVec2 CELL_SIZE=BOX_SIZE+GAP_SIZE;

// No API for centering - see
// https://github.com/ocornut/imgui/issues/897
static const float THUMBNAIL_X=(BOX_SIZE.x-THUMBNAIL_SIZE.x)*.5f;

struct TreeEventAnimState {
    uint64_t state_version=0;
    ImVec2 pos{INFINITY,INFINITY};
    ImVec2 draw_pos{INFINITY,INFINITY};
};

struct TreeEventData {
    size_t te_index=SIZE_MAX;

    TreeEventAnimState *anim_state=nullptr;

    bool can_delete=false;
    bool can_replay=false;
    bool this_timeline=false;
    bool this_window=false;

    BeebWindow *window=nullptr;
    std::shared_ptr<BeebState> state;
    const BeebLoadedConfig *config=nullptr;

    // depth in tree (0=root)
    size_t depth=SIZE_MAX;

    // indexes of children.
    std::vector<size_t> child_idxs;

    // String representation of the state's creation time.
    std::string creation_time;

    // Raw pointer to the thumbnail texture to use - use this when
    // drawing.
    SDL_Texture *thumbnail_texture_raw=nullptr;

    // This data's own thumbnail texture, if it has one.
    SDLUniquePtr<SDL_Texture> thumbnail_texture;

    // Thumbnail generation working data.
    std::shared_ptr<GenerateThumbnailJob> thumbnail_job;
    ThumbnailState thumbnail_state=ThumbnailState_Idle;
    std::string thumbnail_message;
    BeebWindowTextureDataVersion thumbnail_version;
    uint64_t thumbnail_last_update_ticks=0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct LayoutColumn {
    bool above=true;
    int above_counter=0;
    float y=0.f;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TimelineUI:
    public SettingsUI
{
public:
    TimelineUI(BeebWindow *beeb_window,BeebWindowInitArguments init_arguments,SDL_Renderer *renderer,const SDL_PixelFormat *pixel_format);
    ~TimelineUI();

    void DoImGui(CommandContextStack *cc_stack) override;

    bool OnClose() override;
protected:
private:
    Timeline::Tree m_tree;
    std::vector<TreeEventData> m_te_datas;// indexed by the same indexes as
                                          // m_tree.events
    std::map<uint64_t,TreeEventAnimState> m_anim_state_by_id;
    uint64_t m_state_version=0;
    SDL_Renderer *m_renderer=nullptr;
    BeebWindow *m_beeb_window=nullptr;
    SDL_PixelFormat *m_pixel_format=nullptr;
    ImVec2 m_scroll_offset;
    BeebWindowInitArguments m_init_arguments;
    bool m_panning=false;
    std::vector<SDLUniquePtr<SDL_Texture>> m_thumbnail_textures;
    size_t m_max_depth=0;
    std::vector<std::vector<size_t>> m_trees_ordered;

    void DoTreeEventImGui(TreeEventData *te_data,const Timeline::Tree::Event *te,ImDrawList *draw_list,const ImVec2 &origin);
    void UpdateData();
    void InitTreeOrdered(std::vector<size_t> *events,size_t idx,size_t depth);
    ImVec2 DrawLink(ImDrawList *dl,const TreeEventData *src,const TreeEventData *dest,const ImVec2 &origin,const ImColor &colour,float thickness);
    void ImGuiThumbnailImage(TreeEventData *te_data);
    bool UpdateThumbnailTexture(TreeEventData *te_data,const void *pixels);
    SDLUniquePtr<SDL_Texture> GetThumbnailTexture();
    void ReturnThumbnailTexture(SDLUniquePtr<SDL_Texture> texture);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//std::unique_ptr<TimelineUI> TimelineUI::Create() {
//    return std::make_unique<TimelineUI>();
//}

TimelineUI::TimelineUI(BeebWindow *beeb_window,BeebWindowInitArguments init_arguments,SDL_Renderer *renderer,const SDL_PixelFormat *pixel_format):
    m_renderer(renderer),
    m_beeb_window(beeb_window),
    m_pixel_format(ClonePixelFormat(pixel_format)),
    m_init_arguments(std::move(init_arguments))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TimelineUI::~TimelineUI() {
    if(m_pixel_format) {
        SDL_FreeFormat(m_pixel_format);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ThumbnailError(TreeEventData *te_data,const char *message) {
    te_data->thumbnail_state=ThumbnailState_Error;
    te_data->thumbnail_texture=nullptr;
    te_data->thumbnail_texture_raw=nullptr;

    te_data->thumbnail_message=message;

    if(te_data->thumbnail_job) {
        te_data->thumbnail_job->Cancel();
        te_data->thumbnail_job=nullptr;
    }
}

void TimelineUI::DoImGui(CommandContextStack *cc_stack) {
    (void)cc_stack;

    ASSERT(m_pixel_format);

    this->UpdateData();

    ImGui::Text("(timeline data version: %" PRIu64 "; %zu nodes)",m_state_version,m_te_datas.size());

    ImGui::BeginChild("timeline",ImVec2(0,0),true,
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoMove);

    ImDrawList *draw_list=ImGui::GetWindowDrawList();
    draw_list->ChannelsSplit(2);

    // get window origin in screen terms.
    //
    // (Everything is done in screen coordinates, because that's the
    // space in which the low-level drawing stuff seems to operate)
    ImGui::SetCursorPos(ImVec2(0.f,0.f)-m_scroll_offset);
    ImVec2 origin=ImGui::GetCursorScreenPos();

    for(size_t te_idx=0;te_idx<m_tree.events.size();++te_idx) {
        const Timeline::Tree::Event *te=&m_tree.events[te_idx];
        TreeEventData *te_data=&m_te_datas[te_idx];

        this->DoTreeEventImGui(te_data,te,draw_list,origin);
    }

    for(size_t te_idx=0;te_idx<m_tree.events.size();++te_idx) {
        TreeEventData *te_data=&m_te_datas[te_idx];

        te_data->anim_state->draw_pos+=(te_data->anim_state->pos-te_data->anim_state->draw_pos)*.125f;
    }

    draw_list->ChannelsMerge();

    if(ImGui::IsMouseDragging(2,0.0f)) {
        ImGuiIO &io=ImGui::GetIO();

        if(!m_panning) {
            if(ImGui::IsWindowHovered()) {
                m_panning=true;
            }
        }

        if(m_panning) {
            m_scroll_offset-=io.MouseDelta;
        }
    } else {
        m_panning=false;
    }

    ImGui::EndChild();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TimelineUI::OnClose() {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TimelineUI::DoTreeEventImGui(TreeEventData *te_data,const Timeline::Tree::Event *te,ImDrawList *draw_list,const ImVec2 &origin) {
    std::shared_ptr<BeebThread> beeb_thread=m_beeb_window->GetBeebThread();

    ImGuiIDPusher id_pusher(te_data);

    draw_list->ChannelsSetCurrent(1);//fg

    ImGui::SetCursorScreenPos(origin+te_data->anim_state->draw_pos);

    ImGui::BeginChild("frame",BOX_SIZE,false,(ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse));

    if(te_data->window) {
        std::string new_name;
        if(ImGuiInputText(&new_name,"Name",te_data->window->GetName())) {
            te_data->window->SetName(new_name);
        }
    } else if(!!te_data->state) {
        std::string new_name;
        if(ImGuiInputText(&new_name,"Name",te_data->state->GetName())) {
            te_data->state->SetName(new_name);
        }
    } else if(te_data->config) {
        ImGuiInputText(nullptr,"Name",te_data->config->config.name);
    }

    ImGui::LabelText("Creation Time","%s",te_data->creation_time.c_str());

    uint64_t num_cycles=0;
    if(te_data->window) {
        std::shared_ptr<BeebThread> thread=te_data->window->GetBeebThread();
        num_cycles=thread->GetEmulated2MHzCycles();
    } else if(!!te_data->state) {
        num_cycles=te_data->state->GetEmulated2MHzCycles();
    }

    std::string us_str=Get2MHzCyclesString(num_cycles);
    ImGui::LabelText("Run time","%s",us_str.c_str());

    // The thumbnail.
    this->ImGuiThumbnailImage(te_data);

    // Buttons.

    if(ImGuiButton("Load",!te_data->this_window)) {
        std::shared_ptr<BeebThread> thread=m_beeb_window->GetBeebThread();

        if(te_data->this_window) {
            // nowt...
        } else if(te_data->window) {
            std::shared_ptr<BeebThread> src_thread=te_data->window->GetBeebThread();

            src_thread->Send(std::make_unique<BeebThread::CloneThisThreadMessage>(thread));
        } else if(uint64_t id=m_tree.events[te_data->te_index].id) {
            thread->Send(std::make_unique<BeebThread::GoToTimelineNodeMessage>(id));
        }
    }

    ImGui::SameLine();

    if(ImGui::Button("Clone")) {
        if(te_data->window) {
            std::shared_ptr<BeebThread> thread=te_data->window->GetBeebThread();
            thread->Send(std::make_unique<BeebThread::CloneWindowMessage>(m_init_arguments));
        } else {
            BeebWindowInitArguments init_arguments=m_init_arguments;

            init_arguments.initially_paused=false;
            init_arguments.initial_state=te_data->state;
            init_arguments.parent_timeline_event_id=m_tree.events[te_data->te_index].id;

            if(te_data->config) {
                init_arguments.default_config=*te_data->config;
            }

            PushNewWindowMessage(init_arguments);
        }
    }

    ImGui::SameLine();

    if(ImGuiButton("Delete",te_data->can_delete)) {
        Timeline::DeleteEvent(te->id);
        ////BeebState::RemoveFromTimeline(state);
        //state->RemoveFromTimeline();
    }

    ImGui::SameLine();

    if(ImGuiButton("Replay",te_data->can_replay&&!beeb_thread->IsReplaying())) {
        uint64_t id=m_tree.events[te_data->te_index].id;
        beeb_thread->Send(std::make_unique<BeebThread::SaveAndReplayFromMessage>(id));
    }

    ImGui::SameLine();

    if(CanCreateVideoWriter()) {
        if(ImGuiButton("Video",te_data->can_replay)) {
            std::unique_ptr<VideoWriter> video_writer=CreateVideoWriter(m_beeb_window->GetMessageList());

            if(video_writer) {
                SaveFileDialog fd(RECENT_PATHS_VIDEO);
                video_writer->AddFileDialogFilters(&fd);
                fd.AddAllFilesFilter();

                std::string path;
                if(fd.Open(&path)) {
                    fd.AddLastPathToRecentPaths();

                    video_writer->SetFileType(fd.GetFilterIndex());
                    video_writer->SetFileName(path);

                    if(video_writer->BeginWrite()) {
                        uint64_t id=m_tree.events[te_data->te_index].id;
                        beeb_thread->Send(std::make_unique<BeebThread::SaveAndVideoFromMessage>(id,std::move(video_writer)));
                    }
                }
            }
        }
    }
    if(te->id!=0) {
        ImGui::Text("event %" PRIu64,te->id);
    }

    ImGui::EndChild();

    draw_list->ChannelsSetCurrent(0);//bg
    {
        //ImColor background_colour=colours->bg_inactive_colour;

        const ImGuiStyle &style=ImGui::GetStyle();

        ImVec2 ra=origin+te_data->anim_state->draw_pos-style.WindowPadding;
        ImVec2 rb=origin+te_data->anim_state->draw_pos+BOX_SIZE+style.WindowPadding;

        float roundness;
        if(te_data->window) {
            roundness=0.f;
        } else {
            roundness=16.f;
        }

        ImColor colour;
        if(te_data->this_timeline) {
            colour=THIS_TIMELINE_COLOUR;
        } else {
            colour=OTHER_TIMELINE_COLOUR;
        }

        draw_list->AddRectFilled(ra,rb,colour,roundness);
        draw_list->AddRect(ra,rb,ImColor(255,255,255,255),roundness);

        if(te->prev_index!=SIZE_MAX) {
            const TreeEventData *parent_te_data=&m_te_datas[te->prev_index];

            ImVec2 end=this->DrawLink(draw_list,parent_te_data,te_data,origin,TIMELINE_ARROW_COLOUR,TIMELINE_ARROW_THICKNESS);

            ImVec2 t0(end.x+TIMELINE_ARROW_WIDTH*.5f,end.y);
            ImVec2 t1(end.x-TIMELINE_ARROW_WIDTH*.5f,end.y-TIMELINE_ARROW_HEIGHT*.5f);
            ImVec2 t2(end.x-TIMELINE_ARROW_WIDTH*.5f,end.y+TIMELINE_ARROW_HEIGHT*.5f);

            draw_list->AddTriangleFilled(origin+t0,origin+t1,origin+t2,TIMELINE_ARROW_COLOUR);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImVec2 TimelineUI::DrawLink(ImDrawList *dl,const TreeEventData *src,const TreeEventData *dest,const ImVec2 &origin,const ImColor &colour,float thickness) {
    const ImGuiStyle &style=ImGui::GetStyle();

    ImVec2 psrc(src->anim_state->draw_pos.x+BOX_SIZE.x+style.WindowPadding.x,src->anim_state->draw_pos.y+BOX_SIZE.y*.5f);

    ImVec2 pdest(dest->anim_state->draw_pos.x-style.WindowPadding.x,dest->anim_state->draw_pos.y+BOX_SIZE.y*.5f);

    dl->AddBezierCurve(origin+psrc,origin+psrc+ImVec2(50,0),origin+pdest-ImVec2(50,0),origin+pdest,colour,thickness);

    return pdest;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TimelineUI::ImGuiThumbnailImage(TreeEventData *te_data) {
    const char *text=GetThumbnailStateEnumName(te_data->thumbnail_state);

    switch(te_data->thumbnail_state) {
    case ThumbnailState_Idle:
        break;

    case ThumbnailState_Start:
        {
            text="Generating thumbnail...";

            ASSERT(!te_data->thumbnail_texture);
            ASSERT(!te_data->thumbnail_job);

            if(te_data->window) {
                te_data->thumbnail_state=ThumbnailState_Ready;
            } else {
                te_data->thumbnail_job=std::make_shared<GenerateThumbnailJob>();

                bool good_init;
                if(te_data->state) {
                    good_init=te_data->thumbnail_job->Init(te_data->state->CloneBBCMicro(),NUM_THUMBNAIL_RENDER_FRAMES,m_pixel_format);
                } else {
                    const Timeline::Tree::Event *te=&m_tree.events[te_data->te_index];
                    ASSERT(te->be.type==BeebEventType_Root);
                    // initial cycle count doesn't matter here.
                    good_init=te_data->thumbnail_job->Init(te->be.data.config->config.CreateBBCMicro(0),
                                                           NUM_BOOTUP_THUMBNAIL_RENDER_FRAMES,
                                                           m_pixel_format);
                }

                if(!good_init) {
                    ThumbnailError(te_data,"failed to set up thumbnail job");
                    goto ThumbnailState_Error;
                }

                BeebWindows::AddJob(te_data->thumbnail_job);

                te_data->thumbnail_state=ThumbnailState_WaitForThumbnailJob;
            }
        }
        break;

    case ThumbnailState_WaitForThumbnailJob:
        {
            text="Generating thumbnail...";

            ASSERT(!te_data->thumbnail_texture);
            ASSERT(!!te_data->thumbnail_job);

            if(te_data->thumbnail_job->IsFinished()) {
                if(te_data->thumbnail_job->WasCanceled()) {
                    ThumbnailError(te_data,"thumbnail job canceled");
                    goto ThumbnailState_Error;
                }

                const void *pixels=te_data->thumbnail_job->GetTextureData();

                if(UpdateThumbnailTexture(te_data,pixels)) {
                    te_data->thumbnail_state=ThumbnailState_Ready;
                }

                te_data->thumbnail_job=nullptr;
            }
        }
        break;

    case ThumbnailState_Ready:
        {
            ASSERT(!te_data->thumbnail_job);

            if(te_data->window) {
                const SDL_PixelFormat *pixel_format;

                te_data->thumbnail_texture_raw=te_data->window->GetTextureForRenderer(m_renderer);
                if(te_data->thumbnail_texture_raw) {
                    te_data->thumbnail_texture=nullptr;
                } else {
                    const void *pixels;
                    if(te_data->window->GetTextureData(&te_data->thumbnail_version,&pixel_format,&pixels)) {
                        double secs=GetSecondsFromTicks(GetCurrentTickCount()-te_data->thumbnail_last_update_ticks);
                        if(secs>.25) {
                            if(!UpdateThumbnailTexture(te_data,pixels)) {
                                goto ThumbnailState_Error;
                            }
                        }
                    }
                }
            } else {
                ASSERT(!!te_data->thumbnail_texture);
            }
        }
        break;

    ThumbnailState_Error:
    case ThumbnailState_Error:
        {
            ASSERT(!te_data->thumbnail_texture);
            ASSERT(!te_data->thumbnail_job);

            text=te_data->thumbnail_message.c_str();
        }
        break;
    }

    SDL_Texture *thumbnail_texture=nullptr;
    if(te_data->thumbnail_texture_raw) {
        thumbnail_texture=te_data->thumbnail_texture_raw;
    } else if(!!te_data->thumbnail_texture) {
        thumbnail_texture=te_data->thumbnail_texture.get();
    }

    if(thumbnail_texture) {
        ImVec2 pos=ImGui::GetCursorScreenPos();
        pos.x+=THUMBNAIL_X;
        ImGui::SetCursorScreenPos(pos);

        ImGui::Image(thumbnail_texture,THUMBNAIL_SIZE);
    } else {
        ImGui::BeginChild("thumbnail_placeholder",THUMBNAIL_SIZE);
        ImGui::TextUnformatted(text);
        ImGui::EndChild();
    }

    if(ImGui::IsItemVisible()) {
        if(te_data->thumbnail_state==ThumbnailState_Idle) {
            te_data->thumbnail_state=ThumbnailState_Start;
        }
    } else {
        if(te_data->thumbnail_job) {
            te_data->thumbnail_job->Cancel();
            te_data->thumbnail_job=nullptr;
        }

        te_data->thumbnail_state=ThumbnailState_Idle;

        te_data->thumbnail_texture_raw=nullptr;
        this->ReturnThumbnailTexture(std::move(te_data->thumbnail_texture));
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TimelineUI::UpdateThumbnailTexture(TreeEventData *te_data,const void *pixels) {
    if(!te_data->thumbnail_texture) {
        te_data->thumbnail_texture=this->GetThumbnailTexture();
        ASSERT(!!te_data->thumbnail_texture);
    }

    if(SDL_UpdateTexture(te_data->thumbnail_texture.get(),nullptr,pixels,TV_TEXTURE_WIDTH*4)<0) {
        ThumbnailError(te_data,"failed to update texture");
        return false;
    }

    te_data->thumbnail_last_update_ticks=GetCurrentTickCount();

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SDLUniquePtr<SDL_Texture> TimelineUI::GetThumbnailTexture() {
    SDLUniquePtr<SDL_Texture> texture;

    if(m_thumbnail_textures.empty()) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
        texture=SDLUniquePtr<SDL_Texture>(SDL_CreateTexture(m_renderer,m_pixel_format->format,SDL_TEXTUREACCESS_STATIC,TV_TEXTURE_WIDTH,TV_TEXTURE_HEIGHT));
        if(!texture) {
            return nullptr;
        }
    } else {
        texture=std::move(m_thumbnail_textures.back());
        m_thumbnail_textures.pop_back();
    }

    return texture;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TimelineUI::ReturnThumbnailTexture(SDLUniquePtr<SDL_Texture> texture) {
    if(!!texture) {
        m_thumbnail_textures.push_back(std::move(texture));
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TimelineUI::UpdateData() {
    uint64_t state_version=Timeline::GetVersion();

    if(state_version==m_state_version) {
        return;
    }

    m_state_version=state_version;

    m_tree=Timeline::GetTree();

    m_te_datas.clear();
    m_te_datas.resize(m_tree.events.size());

    size_t this_window_idx=SIZE_MAX;

    // Set up the tree.
    for(size_t i=0;i<m_tree.events.size();++i) {
        const Timeline::Tree::Event *te=&m_tree.events[i];
        TreeEventData *te_data=&m_te_datas[i];
        uint64_t te_id=te->id;

        te_data->te_index=i;

        if(te->prev_index!=SIZE_MAX) {
            ASSERT(te->prev_index<i);
            m_te_datas[te->prev_index].child_idxs.push_back(i);
        }

        switch(te->be.type) {
        default:
            ASSERT(!(te->be.GetTypeFlags()&BeebEventTypeFlag_ShownInUI));
            ASSERT(false);
            break;

        case BeebEventType_WindowProxy:
            te_data->window=BeebWindows::FindBeebWindowBySDLWindowID(te->be.data.window_proxy.sdl_window_id);

            if(te_data->window==m_beeb_window) {
                te_data->this_window=true;

                ASSERT(this_window_idx==SIZE_MAX);
                this_window_idx=i;
            }

            // This event wasn't part of the timeline, so it'll have
            // an id of 0. Make up a fake id for it. The actual value
            // isn't important; it just has to be consistent and not
            // conflict with other ids.
            te_id=(1ull<<63)|te->be.data.window_proxy.sdl_window_id;
            break;

        case BeebEventType_SaveState:
            te_data->state=te->be.data.state->state;
            te_data->can_replay=true;
            te_data->can_delete=true;
            break;

        case BeebEventType_Root:
            te_data->config=&te->be.data.config->config;
            te_data->can_replay=true;
            break;
        }

        ASSERT(te_data->can_delete==!!(te->be.GetTypeFlags()&BeebEventTypeFlag_CanDelete));

        te_data->anim_state=&m_anim_state_by_id[te_id];
    }

    // Find nodes that are in this timeline.
    std::vector<size_t> this_timeline_idxs;
    ASSERT(this_window_idx!=SIZE_MAX);
    for(size_t i=this_window_idx;i!=SIZE_MAX;i=m_tree.events[i].prev_index) {
        m_te_datas[i].this_timeline=true;
        this_timeline_idxs.push_back(i);
    }

    std::reverse(this_timeline_idxs.begin(),this_timeline_idxs.end());

    // Arrange items.
    {
        // Get tree nodes in pre-order.
        m_trees_ordered.clear();
        m_trees_ordered.resize(m_tree.root_idxs.size());
        for(size_t i=0;i<m_tree.root_idxs.size();++i) {
            this->InitTreeOrdered(&m_trees_ordered[i],m_tree.root_idxs[i],0);
        }

        // One column state per column.
        std::vector<LayoutColumn> columns;
        columns.resize(m_max_depth+1);

        // For each column, count nodes above the timeline.
        for(size_t tree_idx=0;tree_idx<m_trees_ordered.size();++tree_idx) {
            for(size_t te_idx_idx=0;te_idx_idx<m_trees_ordered[tree_idx].size();++te_idx_idx) {
                size_t te_idx=m_trees_ordered[tree_idx][te_idx_idx];
                const TreeEventData *te_data=&m_te_datas[te_idx];
                LayoutColumn *column=&columns[te_data->depth];

                if(te_data->depth>=this_timeline_idxs.size()) {
                    // Copy previous column's flag.
                    ASSERT(te_data->depth>0);
                    column->above=columns[te_data->depth-1].above;
                } else {
                    if(column->above) {
                        if(te_idx==this_timeline_idxs[te_data->depth]) {
                            column->above=false;
                        }
                    } else {
                        ASSERT(te_idx!=this_timeline_idxs[te_data->depth]);
                    }
                }

                // 
                if(column->above) {
                    --column->above_counter;
                }
            }
        }

        // If there are nodes above the timeline, shift that column
        // upwards; if there's nothing , and this column is past the
        // end of the timeline, shift it downwards by one row. The Y=0
        // row then contains the current timeline, and nothing else.
        for(size_t i=0;i<columns.size();++i) {
            LayoutColumn *column=&columns[i];

            if(i>=this_timeline_idxs.size()&&column->above_counter==0) {
                column->y+=CELL_SIZE.y;
            } else {
                column->y+=column->above_counter*CELL_SIZE.y;
            }
        }

        // Arrange nodes. 
        // 
        /// The width of each node is taken into account, but
        /// everything is actually laid out on a grid.
        for(size_t tree_idx=0;tree_idx<m_trees_ordered.size();++tree_idx) {
            for(size_t te_idx_idx=0;te_idx_idx<m_trees_ordered[tree_idx].size();++te_idx_idx) {
                size_t te_idx=m_trees_ordered[tree_idx][te_idx_idx];
                TreeEventData *te_data=&m_te_datas[te_idx];
                TreeEventAnimState *as=te_data->anim_state;
                LayoutColumn *column=&columns[te_data->depth];

                as->pos.x=te_data->depth*CELL_SIZE.x;
                as->pos.y=column->y;
                column->y+=CELL_SIZE.y;

                // Skip the Y=0 row when necessary.
                if(++column->above_counter==0&&te_data->depth>=this_timeline_idxs.size()) {
                    column->y+=CELL_SIZE.y;
                }

                if(as->state_version==0) {
                    // New thing... just have it pop into place.
                    as->draw_pos=as->pos;
                }

                as->state_version=m_state_version;

            }
        }

        // Remove stale anim states.
        {
            auto it=m_anim_state_by_id.begin();
            while(it!=m_anim_state_by_id.end()) {
                if(it->second.state_version!=m_state_version) {
                    it=m_anim_state_by_id.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TimelineUI::InitTreeOrdered(std::vector<size_t> *events,size_t idx,size_t depth) {
    TreeEventData *te_data=&m_te_datas[idx];

    m_max_depth=std::max(m_max_depth,depth);

    ASSERT(te_data->depth==SIZE_MAX);
    te_data->depth=depth;

    events->push_back(idx);

    for(size_t child_idx:te_data->child_idxs) {
        this->InitTreeOrdered(events,child_idx,depth+1);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateTimelineUI(BeebWindow *beeb_window,BeebWindowInitArguments init_arguments,SDL_Renderer *renderer,const SDL_PixelFormat *pixel_format) {
    return std::make_unique<TimelineUI>(beeb_window,std::move(init_arguments),renderer,pixel_format);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
