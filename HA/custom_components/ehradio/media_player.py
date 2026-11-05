import logging
import voluptuous as vol
import json
import urllib.request
import asyncio

from homeassistant.components import mqtt, media_source
from homeassistant.components.media_player.browse_media import async_process_play_media_url
from homeassistant.const import CONF_NAME
from homeassistant.helpers import config_validation as cv
from homeassistant.helpers.device_registry import DeviceInfo

from homeassistant.components.media_player import (
    PLATFORM_SCHEMA as MEDIA_PLAYER_PLATFORM_SCHEMA,
    BrowseMedia,
    MediaPlayerEntity,
    MediaPlayerEntityFeature,
    MediaPlayerState,
    MediaPlayerEnqueue,
    MediaType,
    RepeatMode,
)

VERSION = '2026.06.03'

_LOGGER      = logging.getLogger(__name__)

SUPPORT_EHRADIO = (
    MediaPlayerEntityFeature.PAUSE
    | MediaPlayerEntityFeature.PLAY
    | MediaPlayerEntityFeature.STOP
    | MediaPlayerEntityFeature.VOLUME_SET
    | MediaPlayerEntityFeature.VOLUME_STEP
    | MediaPlayerEntityFeature.TURN_OFF
    | MediaPlayerEntityFeature.TURN_ON
    
    | MediaPlayerEntityFeature.PREVIOUS_TRACK
    | MediaPlayerEntityFeature.NEXT_TRACK
    | MediaPlayerEntityFeature.SELECT_SOURCE
    | MediaPlayerEntityFeature.BROWSE_MEDIA
    | MediaPlayerEntityFeature.PLAY_MEDIA
)

DEFAULT_NAME = 'myradio'
CONF_MAX_VOLUME = 'max_volume'
CONF_ROOT_TOPIC = 'root_topic'
CONF_FALLBACK_IMAGE = 'fallback_image'
CONF_DEVICE_URL = 'device_url'
DEFAULT_FALLBACK_IMAGE = 'https://trip5.github.io/ehRadio/images/logo-color-square.png'

MEDIA_PLAYER_PLATFORM_SCHEMA = MEDIA_PLAYER_PLATFORM_SCHEMA.extend({
  vol.Required(CONF_ROOT_TOPIC, default="ehradio"): cv.string,
  vol.Optional(CONF_NAME, default=DEFAULT_NAME): cv.string,
  vol.Optional(CONF_MAX_VOLUME, default='42'): cv.string,
  vol.Optional(CONF_FALLBACK_IMAGE, default=DEFAULT_FALLBACK_IMAGE): cv.string,
  vol.Optional(CONF_DEVICE_URL, default=''): cv.string
})

def setup_platform(hass, config, add_devices, discovery_info=None):
  root_topic = config.get(CONF_ROOT_TOPIC)
  name = config.get(CONF_NAME)
  max_volume = int(config.get(CONF_MAX_VOLUME, 42))
  fallback_image = config.get(CONF_FALLBACK_IMAGE, DEFAULT_FALLBACK_IMAGE)
  device_url = config.get(CONF_DEVICE_URL, '')
  playlist = []
  api = ehradioApi(root_topic, hass, playlist, device_url)
  add_devices([ehradioDevice(name, max_volume, fallback_image, device_url, api)], True)

class ehradioApi():
  def __init__(self, root_topic, hass, playlist, device_url):
    self.hass = hass
    self.mqtt = mqtt
    self.root_topic = root_topic.strip('/')
    self.playlist = playlist
    self.playlisturl = ""
    self.device_url = device_url  # user-configured; auto-detected from playlist if empty

  async def set_command(self, command):
    try:
      self.mqtt.async_publish(self.root_topic + '/command', command)
    except:
      await self.mqtt.async_publish(self.hass, self.root_topic + '/command', command)

  async def set_volume(self, volume):
    command = "vol " + str(volume)
    try:
      self.mqtt.async_publish(self.root_topic + '/command', command)
    except:
      await self.mqtt.async_publish(self.hass, self.root_topic + '/command', command)
      
  def fetch_data(self):
    try:
      html = urllib.request.urlopen(self.playlisturl).read().decode("utf-8")
      return str(html)
    except Exception as e:
      _LOGGER.error(f"Unable to fetch playlist from {self.playlisturl}: " + str(e))
      return ""
        
  async def set_source(self, source):
    number = source.split('.')
    command = "play " + number[0]
    try:
      self.mqtt.async_publish(self.root_topic + '/command', command)
    except:
      await self.mqtt.async_publish(self.hass, self.root_topic + '/command', command)

  async def set_browse_media(self, media_content_id):
    try:
      self.mqtt.async_publish(self.root_topic + '/command', media_content_id)
    except:
      await self.mqtt.async_publish(self.hass, self.root_topic + '/command', media_content_id)
      
  async def load_playlist(self, msg):
    try:
      self.playlisturl = msg.payload
      # Auto-detect device URL from playlist URL if not configured
      if not self.device_url and self.playlisturl.startswith('http'):
        from urllib.parse import urlparse
        parsed = urlparse(self.playlisturl)
        self.device_url = f"{parsed.scheme}://{parsed.netloc}/"
      file = await self.hass.async_add_executor_job(self.fetch_data)
    except uException as e:
      _LOGGER.error(f"Error load_playlist from {self.playlisturl}")
    else:
      file = file.split('\n')
      counter = 1
      self.playlist.clear()
      for line in file:
        res = line.split('\t')
        if res[0] != "":
          station = str(counter) + '. ' + res[0]
          self.playlist.append(station)
          counter=counter+1

class ehradioDevice(MediaPlayerEntity):
  def __init__(self, name, max_volume, fallback_image, device_url, api):
    self._name = name
    self.api = api
    self._state = MediaPlayerState.OFF
    self._current_source = None
    self._media_title = ''
    self._track_artist = ''
    self._track_album_name = ''
    self._entity_picture = None
    self._volume = 0
    self._max_volume = max_volume
    self._fallback_image = fallback_image
    self._device_url = device_url

  @property
  def device_info(self) -> DeviceInfo:
    device_url = self.api.device_url or self._device_url
    return DeviceInfo(
        identifiers={("ehradio", self._name)},
        name=self._name,
        manufacturer="ehRadio",
        model="ESP32 Internet Radio",
        configuration_url=device_url if device_url else None,
    )

  async def async_added_to_hass(self):
    await asyncio.sleep(5)
    await mqtt.async_subscribe(self.api.hass, self.api.root_topic+'/status', self.status_listener, 0, "utf-8")
    await mqtt.async_subscribe(self.api.hass, self.api.root_topic+'/playlist', self.playlist_listener, 0, "utf-8")
    await mqtt.async_subscribe(self.api.hass, self.api.root_topic+'/volume', self.volume_listener, 0, "utf-8")
    
  async def status_listener(self, msg):
    try:
      js = json.loads(msg.payload)
      station_name = js.get('name', '')
      track_title = js.get('title', '')
      self._media_title = station_name or track_title
      self._track_artist = track_title if station_name else None
      if js['on']==1:
        self._state = MediaPlayerState.PLAYING if js['status']==1 else MediaPlayerState.IDLE
      else:
        self._state = MediaPlayerState.PLAYING if js['status']==1 else MediaPlayerState.OFF
      self._current_source = str(js['station']) + '. ' + js['name']
      self._entity_picture = js.get('image_url') or None
      self._max_volume = js.get('max_volume', self._max_volume)
      try:
        self.async_schedule_update_ha_state()
      except:
        pass
    except:
      pass

  async def playlist_listener(self, msg):
    await self.api.load_playlist(msg)
    try:
      self.async_schedule_update_ha_state()
    except:
      pass

  async def volume_listener(self, msg):
    self._volume = int(msg.payload) / self._max_volume
    try:
      self.async_schedule_update_ha_state()
    except:
      pass

  @property
  def supported_features(self):
    return SUPPORT_EHRADIO

  @property
  def name(self):
    return self._name

  @property
  def media_title(self):
    return self._media_title

  @property
  def media_content_type(self):
    return MediaType.MUSIC

  @property
  def media_artist(self):
    return self._track_artist

  @property
  def entity_picture(self):
    return self._entity_picture if self._entity_picture else self._fallback_image

  @property
  def media_image_remotely_accessible(self):
    return True

  @property
  def unique_id(self):
    return f"ehradio_{self._name}"

  @property
  def extra_state_attributes(self):
    attrs = {}
    device_url = self.api.device_url or self._device_url
    if device_url:
        attrs["device_url"] = device_url
    return attrs

  @property
  def state(self):
    return self._state

  @property
  def volume_level(self):
    return self._volume

  async def async_set_volume_level(self, volume):
    await self.api.set_volume(round(volume * self._max_volume,1))

  @property
  def source(self):
    return self._current_source

  @property
  def source_list(self):
    return self.api.playlist

  async def async_browse_media(
    self, media_content_type: str | None = None, media_content_id: str | None = None
  ) -> BrowseMedia:
    return await media_source.async_browse_media(
      self.hass,
      media_content_id,
    )

  async def async_play_media(
    self,
    media_type: str,
    media_id: str,
    enqueue: MediaPlayerEnqueue | None = None,
    announce: bool | None = None, **kwargs
  ) -> None:
    if media_source.is_media_source_id(media_id):
      media_type = MediaType.URL
      play_item = await media_source.async_resolve_media(self.hass, media_id, self.entity_id)
      media_id = async_process_play_media_url(self.hass, play_item.url)
    await self.api.set_browse_media(media_id)
    
  async def async_select_source(self, source):
    await self.api.set_source(source)
    self._current_source = source

  async def async_volume_up(self):
      newVol = float(self._volume) + 0.05
      await self.async_set_volume_level(newVol)
      self._volume = newVol

  async def async_volume_down(self):
      newVol = float(self._volume) - 0.05
      await self.async_set_volume_level(newVol)
      self._volume = newVol

  async def async_media_next_track(self):
      await self.api.set_command("next")

  async def async_media_previous_track(self):
      await self.api.set_command("prev")

  async def async_media_stop(self):
      await self.api.set_command("stop")
      self._state = MediaPlayerState.IDLE

  async def async_media_play(self):
      await self.api.set_command("start")
      self._state = MediaPlayerState.PLAYING

  async def async_media_pause(self):
      await self.api.set_command("stop")
      self._state = MediaPlayerState.IDLE
  
  async def async_turn_off(self):
      await self.api.set_command("turnoff")
      self._state = MediaPlayerState.OFF

  async def async_turn_on(self, **kwargs):
      await self.api.set_command("turnon")
      self._state = MediaPlayerState.ON
