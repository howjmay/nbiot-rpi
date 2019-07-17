#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "client_common.h"
#include "mosquitto.h"
#include "mqtt_protocol.h"

enum prop_type {
  PROP_TYPE_BYTE,
  PROP_TYPE_INT16,
  PROP_TYPE_INT32,
  PROP_TYPE_BINARY,
  PROP_TYPE_STRING,
  PROP_TYPE_STRING_PAIR
};

/* This parses property inputs. It should work for any command type, but is limited at the moment.
 *
 * Format:
 *
 * command property value
 * command property key value
 *
 * Example:
 *
 * publish message-expiry-interval 32
 * connect user-property key value
 */

int cfg_parse_property(mosq_config_t *cfg, int argc, char *argv[], int *idx) {
  char *cmdname = NULL, *propname = NULL;
  char *key = NULL, *value = NULL;
  int cmd, identifier, type;
  mosquitto_property **proplist;
  int rc;

  /* idx now points to "command" */
  if ((*idx) + 2 > argc - 1) {
    /* Not enough args */
    fprintf(stderr, "Error: --property argument given but not enough arguments specified.\n\n");
    return MOSQ_ERR_INVAL;
  }

  cmdname = argv[*idx];
  if (mosquitto_string_to_command(cmdname, &cmd)) {
    fprintf(stderr, "Error: Invalid command given in --property argument.\n\n");
    return MOSQ_ERR_INVAL;
  }

  propname = argv[(*idx) + 1];
  if (mosquitto_string_to_property_info(propname, &identifier, &type)) {
    fprintf(stderr, "Error: Invalid property name given in --property argument.\n\n");
    return MOSQ_ERR_INVAL;
  }

  if (mosquitto_property_check_command(cmd, identifier)) {
    fprintf(stderr, "Error: %s property not allow for %s in --property argument.\n\n", propname, cmdname);
    return MOSQ_ERR_INVAL;
  }

  if (identifier == MQTT_PROP_USER_PROPERTY) {
    if ((*idx) + 3 > argc - 1) {
      /* Not enough args */
      fprintf(stderr, "Error: --property argument given but not enough arguments specified.\n\n");
      return MOSQ_ERR_INVAL;
    }

    key = argv[(*idx) + 2];
    value = argv[(*idx) + 3];
    (*idx) += 3;
  } else {
    value = argv[(*idx) + 2];
    (*idx) += 2;
  }

  switch (cmd) {
    case CMD_CONNECT:
      proplist = &cfg->property_config->connect_props;
      break;

    case CMD_PUBLISH:
      if (identifier == MQTT_PROP_TOPIC_ALIAS) {
        cfg->pub_config->have_topic_alias = true;
      }
      if (identifier == MQTT_PROP_SUBSCRIPTION_IDENTIFIER) {
        fprintf(stderr, "Error: %s property not supported for %s in --property argument.\n\n", propname, cmdname);
        return MOSQ_ERR_INVAL;
      }
      proplist = &cfg->property_config->publish_props;
      break;

    case CMD_SUBSCRIBE:
      if (identifier != MQTT_PROP_SUBSCRIPTION_IDENTIFIER && identifier != MQTT_PROP_USER_PROPERTY) {
        fprintf(stderr, "Error: %s property not supported for %s in --property argument.\n\n", propname, cmdname);
        return MOSQ_ERR_NOT_SUPPORTED;
      }
      proplist = &cfg->property_config->subscribe_props;
      break;

    case CMD_UNSUBSCRIBE:
      proplist = &cfg->property_config->unsubscribe_props;
      break;

    case CMD_DISCONNECT:
      proplist = &cfg->property_config->disconnect_props;
      break;

    case CMD_AUTH:
      fprintf(stderr, "Error: %s property not supported for %s in --property argument.\n\n", propname, cmdname);
      return MOSQ_ERR_NOT_SUPPORTED;

    case CMD_WILL:
      proplist = &cfg->property_config->will_props;
      break;

    case CMD_PUBACK:
    case CMD_PUBREC:
    case CMD_PUBREL:
    case CMD_PUBCOMP:
    case CMD_SUBACK:
    case CMD_UNSUBACK:
      fprintf(stderr, "Error: %s property not supported for %s in --property argument.\n\n", propname, cmdname);
      return MOSQ_ERR_NOT_SUPPORTED;

    default:
      return MOSQ_ERR_INVAL;
  }

  switch (type) {
    case MQTT_PROP_TYPE_BYTE:
      rc = mosquitto_property_add_byte(proplist, identifier, atoi(value));
      break;
    case MQTT_PROP_TYPE_INT16:
      rc = mosquitto_property_add_int16(proplist, identifier, atoi(value));
      break;
    case MQTT_PROP_TYPE_INT32:
      rc = mosquitto_property_add_int32(proplist, identifier, atoi(value));
      break;
    case MQTT_PROP_TYPE_VARINT:
      rc = mosquitto_property_add_varint(proplist, identifier, atoi(value));
      break;
    case MQTT_PROP_TYPE_BINARY:
      rc = mosquitto_property_add_binary(proplist, identifier, value, strlen(value));
      break;
    case MQTT_PROP_TYPE_STRING:
      rc = mosquitto_property_add_string(proplist, identifier, value);
      break;
    case MQTT_PROP_TYPE_STRING_PAIR:
      rc = mosquitto_property_add_string_pair(proplist, identifier, key, value);
      break;
    default:
      return MOSQ_ERR_INVAL;
  }
  if (rc) {
    fprintf(stderr, "Error adding property %s %d\n", propname, type);
    return rc;
  }
  return MOSQ_ERR_SUCCESS;
}
