#include "npc_interact.h"
#include "lib/System.h"
#include <esp_http_client.h>
#include <cJSON.h>

const char *HTTPTAG = "httpclient";
std::string HttpResponseStr;

esp_err_t _http_event_handle(esp_http_client_event_t *evt) {
	switch(evt->event_id) {
		case HTTP_EVENT_ERROR:
			ESP_LOGI(HTTPTAG, "HTTP_EVENT_ERROR");
			break;
		case HTTP_EVENT_ON_CONNECTED:
			ESP_LOGI(HTTPTAG, "HTTP_EVENT_ON_CONNECTED");
			break;
		case HTTP_EVENT_HEADER_SENT:
			ESP_LOGI(HTTPTAG, "HTTP_EVENT_HEADER_SENT");
			break;
		case HTTP_EVENT_ON_HEADER:
			ESP_LOGI(HTTPTAG, "HTTP_EVENT_ON_HEADER");
			//printf("%.*s", evt->data_len, (char*)evt->data);
			break;
		case HTTP_EVENT_ON_DATA:
			ESP_LOGI(HTTPTAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
			if (!esp_http_client_is_chunked_response(evt->client)) {
				//ESP_LOGI(HTTPTAG, "%.*s", evt->data_len, (char*)evt->data);
				printf("%.*s", evt->data_len, (char*)evt->data);
				HttpResponseStr.append((const char *)evt->data, evt->data_len);
			}
			break;
		case HTTP_EVENT_ON_FINISH:
			ESP_LOGI(HTTPTAG, "HTTP_EVENT_ON_FINISH");
			break;
		case HTTP_EVENT_DISCONNECTED:
			ESP_LOGI(HTTPTAG, "HTTP_EVENT_DISCONNECTED");
			break;
		}
	return ESP_OK;
}


static StaticQueue_t InQueue;
const char *NPCInteractionTask::LOGTAG = "NPCITTask";
static uint8_t CommandBuffer[NPCInteractionTask::NPCMSG_QUEUE_SIZE * 
		NPCInteractionTask::NPCMSG_ITEM_SIZE] = { 0 };
	
NPCInteractionTask::NPCInteractionTask(const std::string &tName, 
	uint16_t stackSize, uint8_t p) : Task(tName,stackSize,p), InQueueHandle(nullptr) {

}

bool NPCInteractionTask::init() {
	InQueueHandle = xQueueCreateStatic(NPCMSG_QUEUE_SIZE, NPCMSG_ITEM_SIZE, 
						 &CommandBuffer[0], &InQueue);
	if (InQueueHandle == nullptr) {
		ESP_LOGI(LOGTAG, "Failed creating incomming queue");
	}
	return true;
}

void helo(QueueHandle_t RetQ) {
	HttpResponseStr.clear();
	esp_http_client_config_t config;
	memset(&config,0,sizeof(config));
  	config.url = "http://192.168.4.1:8080/npc";
	config.event_handler = _http_event_handle;

	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_err_t err = esp_http_client_perform(client);

	uint8_t wasError = 0;
	cJSON *root = 0;
	NPCIResponse* npcir = (NPCIResponse*)malloc(sizeof(NPCIResponse));
	char** strings = NULL;
	if (err == ESP_OK) {
		ESP_LOGI(HTTPTAG, "Status = %d, content_length = %d",
		esp_http_client_get_status_code(client),
		esp_http_client_get_content_length(client));
		ESP_LOGI(HTTPTAG, "%s", HttpResponseStr.c_str());
		ESP_LOGI(HTTPTAG, "parising json");
		root = cJSON_Parse(HttpResponseStr.c_str());
		if(cJSON_IsArray(root->child)) {
			ESP_LOGI(HTTPTAG, "child is array");
			int size = cJSON_GetArraySize(root->child);
			if (size > 0)
				strings = (char**)malloc(sizeof(char*)*size);
			for(int i=0;i<size;i++) {
				cJSON *item = cJSON_GetArrayItem(root->child, i);
				ESP_LOGI(HTTPTAG,"item %d, %s", i, cJSON_GetStringValue(item));
				strings[i] = strdup(cJSON_GetStringValue(item));
			}
			npcir->numstrings = size;
			npcir->strings = strings;
		} else {
			wasError = 1;
		}
	} else {
		wasError = 1;
	}

	npcir->type = NPCI_NAME_RESP;
	npcir->wasError = wasError;
	xQueueSend(RetQ, (void*)&npcir, (TickType_t ) 100);

	if(root) cJSON_Delete(root);
	esp_http_client_cleanup(client);
}

void interact(NPCInteractionTask::NPCMsg *m) {
	HttpResponseStr.clear();
	esp_http_client_config_t config;
	std::string url = "http://192.168.4.1:8080/npc/";
	url+=m->NpcName;
	if(m->Action[0]!='\0') {
		url.append("/");
		url.append(m->Action);
	}
	
	memset(&config,0,sizeof(config));
  	config.url = url.c_str();
	config.event_handler = _http_event_handle;

	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_err_t err = esp_http_client_perform(client);

	uint8_t wasError = 0;
	uint32_t infections=0;
	cJSON *root = 0;
	NPCIResponse* npcir = (NPCIResponse*)malloc(sizeof(NPCIResponse));
	char** actions = NULL;
	if (err == ESP_OK) {
		ESP_LOGI(HTTPTAG, "Status = %d, content_length = %d",
		esp_http_client_get_status_code(client),
		esp_http_client_get_content_length(client));
		ESP_LOGI(HTTPTAG, "%s", HttpResponseStr.c_str());
		ESP_LOGI(HTTPTAG, "parising json");
		root = cJSON_Parse(HttpResponseStr.c_str());
		cJSON *a = cJSON_GetObjectItem(root,(const char *)"a");
		cJSON *d = cJSON_GetObjectItem(root,(const char *)"d");
		cJSON *ji = cJSON_GetObjectItem(root,(const char *)"i");
		cJSON *n = cJSON_GetObjectItem(root,(const char *)"n");
		cJSON *r = cJSON_GetObjectItem(root,(const char *)"r");
		if(a && d && ji && n && cJSON_IsArray(a) && cJSON_IsArray(ji)) {
			ESP_LOGI(HTTPTAG, "actions and infects is an arrary");
			int size = cJSON_GetArraySize(a);
			if (size > 0)
				actions = (char**)malloc(sizeof(char*)*size);
			for(int i=0;i<size;i++) {
				cJSON *item = cJSON_GetArrayItem(a, i);
				ESP_LOGI(HTTPTAG,"item %d, %s", i, cJSON_GetStringValue(item));
				actions[i] = strdup(cJSON_GetStringValue(item));
			}
			npcir->numstrings = size;
			npcir->strings = actions;

			size = cJSON_GetArraySize(ji);
			for(int i=0;i<size;i++) {
				infections|=cJSON_GetArrayItem(ji,i)->valueint;
			}
			npcir->infections = infections;

			npcir->name = strdup(cJSON_GetStringValue(n));
			npcir->desc = strdup(cJSON_GetStringValue(d));
			npcir->resp = strdup(cJSON_GetStringValue(r));
		} else {
			wasError = 1;
		}
	} else {
		wasError = 1;
	}

	npcir->wasError = wasError;
	xQueueSend(m->RetQ, (void*)&npcir, (TickType_t ) 100);

	if(root) cJSON_Delete(root);
	esp_http_client_cleanup(client);
}


void NPCInteractionTask::run(void *data) {
	ESP_LOGI(LOGTAG, "NPCInteractionTask started");
	while (1) {
		NPCMsg *m=0;
		if (xQueueReceive(getQueueHandle(), &m, (TickType_t) 1000 / portTICK_PERIOD_MS)) {
			if(m->RType==NPCMsg::HELO) {
				ESP_LOGI(LOGTAG,"got HELO");
				helo(m->RetQ);
			} else if (m->RType==NPCMsg::INTERACT) {
				ESP_LOGI(LOGTAG,"got Interact");
				interact(m);
			}
			delete m;
		}
	}
}

NPCInteractionTask::~NPCInteractionTask() { }

