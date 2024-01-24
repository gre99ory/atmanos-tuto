#include <furi.h>
#include <gui/gui.h>
#include <stdlib.h>
#include <input/input.h>

/**
 * Point structure (data model)
 */
typedef struct {
    int x;
    int y;
} BoxMoverModel;

/**
 * Application structure
 */
typedef struct {
    BoxMoverModel* model;
    // Mutex for concurrent acces to model
    FuriMutex** model_mutex;

    FuriMessageQueue* event_queue;

    ViewPort* view_port;
    Gui* gui;
} App;

/************************************************
 * Callbacks
 */

/**
 * Input callback
 */
void input_callback(InputEvent* input, void* ctx) {
    App* box_mover = ctx;
    // Puts input onto event queue with priority 0, and waits until completion.
    // osMessageQueuePut(box_mover->event_queue, input, 0, osWaitForever);
    furi_message_queue_put(box_mover->event_queue, input, 0);
}

/**
 * Draw callback
 */
void draw_callback(Canvas* canvas, void* ctx) {
    App* box_mover = ctx;
    // furi_check(osMutexAcquire(box_mover->model_mutex, osWaitForever) == osOK);
    furi_check(furi_mutex_acquire(box_mover->model_mutex, FuriWaitForever) == FuriStatusOk);

    FuriString* buffer;
    buffer = furi_string_alloc();
    canvas_set_font(canvas, FontPrimary);

    furi_string_printf(buffer, "x=%d", box_mover->model->x);
    canvas_draw_str_aligned(canvas, 30, 20, AlignLeft, AlignTop, furi_string_get_cstr(buffer));

    furi_string_printf(buffer, "y=%d", box_mover->model->y);
    canvas_draw_str_aligned(canvas, 30, 30, AlignLeft, AlignTop, furi_string_get_cstr(buffer));

    furi_string_free(buffer);

    canvas_draw_box(
        canvas, box_mover->model->x, box_mover->model->y, 4, 4); // Draw a box on the screen

    furi_mutex_release(box_mover->model_mutex);
}

/**
 * Event loop
*/
void event_loop(App* box_mover) {
    InputEvent event;
    for(bool processing = true; processing;) {
        // Pops a message off the queue and stores it in `event`.
        // No message priority denoted by NULL, and 100 ticks of timeout.

        // osStatus_t status = osMessageQueueGet(, NULL, 100);
        FuriStatus status =
            furi_message_queue_get(box_mover->event_queue, &event, FuriWaitForever);

        // furi_check(osMutexAcquire(box_mover->model_mutex, osWaitForever) == osOK);
        furi_check(furi_mutex_acquire(box_mover->model_mutex, FuriWaitForever) == FuriStatusOk);

        if(status == FuriStatusOk) {
            if(event.type == InputTypePress) {
                switch(event.key) {
                case InputKeyUp:
                    box_mover->model->y -= 2;
                    break;
                case InputKeyDown:
                    box_mover->model->y += 2;
                    break;
                case InputKeyLeft:
                    box_mover->model->x -= 2;
                    break;
                case InputKeyRight:
                    box_mover->model->x += 2;
                    break;
                case InputKeyOk:
                case InputKeyBack:
                    processing = false;
                    break;
                default:
                    break;
                }
            }
        }
        furi_mutex_release(box_mover->model_mutex);
        view_port_update(box_mover->view_port); // signals our draw callback
    }
}

/************************************************
 * Memory management - Allocation
 */
App* box_mover_alloc() {
    // 1-Allocate app
    App* instance = malloc(sizeof(App));

    // 2-Allocate model
    instance->model = malloc(sizeof(BoxMoverModel));
    instance->model->x = 10;
    instance->model->y = 10;

    // 3-Get a GUI pointer & add view port
    instance->gui = furi_record_open("gui");

    // 4-Allocate view_port & attach to gui
    instance->view_port = view_port_alloc();
    view_port_draw_callback_set(instance->view_port, draw_callback, instance);
    view_port_input_callback_set(instance->view_port, input_callback, instance);
    gui_add_view_port(instance->gui, instance->view_port, GuiLayerFullscreen);

    // 5-Message queue
    instance->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // 6-Mutex
    instance->model_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    return instance;
}

/**
 * Memory management - Free
 */
void box_mover_free(App* instance) {
    // 6-Mutex
    furi_mutex_free(instance->model_mutex);

    // 5-Message queue
    furi_message_queue_free(instance->event_queue);

    // 4-View port
    view_port_enabled_set(instance->view_port, false); // Disables our ViewPort
    gui_remove_view_port(instance->gui, instance->view_port); // Removes our ViewPort from the Gui
    view_port_free(instance->view_port); // Frees memory allocated by view_port_alloc

    // 3-GUI
    furi_record_close("gui"); // Closes the gui record

    // 2-Model
    free(instance->model);

    // 1-App
    free(instance);
}

/**
 * Main entry point
 */
int32_t box_mover_app(void* p) {
    UNUSED(p);
    App* box_mover = box_mover_alloc();

    event_loop(box_mover);

    box_mover_free(box_mover);
    return 0;
}