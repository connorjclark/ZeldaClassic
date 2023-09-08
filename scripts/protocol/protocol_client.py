# python -m pip install websocket-client

import time
import json
from typing import Dict
from websockets.sync.client import connect
from websockets.exceptions import ConnectionClosedError


class ProtocolError(Exception):
	pass


class ZCProtocol():
	def __init__(self, url):
		self.url = url
		self.next_command_id = 1
		self.command_responses = []
		self.events = []

		attempts = 0
		max_attempts = 10
		retry_sec = 1
		while True:
			try:
				self.ws = connect(url, open_timeout=20)
				break
			except Exception as e:
				attempts += 1
				if attempts == max_attempts:
					raise e
				time.sleep(retry_sec)

	def alive(self):
		return True

	# async def start(self):
	#     pass
		# async with websocket.connect(self.url) as ws:
		#     name = 'connor'

		#     await ws.send(name)
		#     print(f">>> {name}")

		#     greeting = await ws.recv()
		#     print(f"<<< {greeting}")

	def send_command(self, type: str, params: Dict = {}):
		id = self.send_command_async(type, params)
		return self.wait_for_command(id)

	def send_command_async(self, type: str, params: Dict = {}):
		id = self.next_command_id
		self.next_command_id += 1
		data = {
			'id': id,
			'type': type,
			'params': params,
		}
		self.ws.send(json.dumps(data))
		return id

	def wait_for_command(self, id: int):
		for response in self.command_responses:
			if response['id'] == id:
				self.command_responses.remove(response)
				if 'error' in response:
					raise ProtocolError(response)
				return response

		while self.alive():
			message = self.wait_for_message()
			if not message:
				raise ProtocolError('Connection closed')

			response_id = message.get('id', None)
			if response_id == id:
				if 'error' in message:
					raise ProtocolError(message)
				return message
			elif response_id == None:
				self.events.append(message)
			else:
				self.command_responses.append(message)

	def pop_event(self):
		return self.events.pop(0)

	def clear_events(self):
		self.events.clear()

	def wait_for_event(self, method: str):
		for event in self.events:
			if event['method'] == method:
				self.events.remove(event)
				return event

		while self.alive():
			message = self.wait_for_message()
			if not message:
				return None

			id = message.get('id', None)
			msg_method = message.get('method', None)
			if id == None and msg_method == method:
				return message
			elif id == None and msg_method != None:
				self.events.append(message)
			else:
				self.command_responses.append(message)

		return None

	def wait_for_any_event(self):
		if self.events:
			return True

		while self.alive():
			message = self.wait_for_message()
			if not message:
				return False

			id = message.get('id', None)
			method = message.get('method', None)
			if id == None and method != None:
				self.events.append(message)
				break
			else:
				self.command_responses.append(message)

		return True
	
	def wait_for_message(self):
		try:
			data = self.ws.recv()
			message = json.loads(data)
			return message
		except ConnectionClosedError:
			return None
