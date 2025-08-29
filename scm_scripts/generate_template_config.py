import configparser
config = configparser.ConfigParser()
config['DEFAULT'] = {}
config['DEFAULT']['APIKEY'] = "Enter API-KEY here"
config['DEFAULT']['PROBE_LAT'] = "40.350288669177814"
config['DEFAULT']['PROBE_LNG'] = "-74.65208898358767"
config['DEFAULT']['USRP_CAL_OFFSET'] = "24"
config['DEFAULT']['EXTERNAL_GAIN_OFFSET'] = "0"




with open('template.ini', 'w') as configfile:
  config.write(configfile)