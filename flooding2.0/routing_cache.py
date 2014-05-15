class routing_cache(object):
	size = 4
	min_size = 1

	def __init__(self):
		self.cache = { }

		self.recent_size = routing_cache.size / 2
		self.recent = [ ]
		self.ghost_recent = [ ]

		self.frequent = [ ]
		self.ghost_frequent = [ ]

	def assert_sizes(self):
		assert len(self.cache) <= routing_cache.size
		assert len(self.ghost_recent) <= routing_cache.size / 2
		assert len(self.ghost_frequent) <= routing_cache.size / 2
		assert self.recent_size < routing_cache.size
		assert self.recent_size >= routing_cache.min_size
		assert len(self.recent) <= self.recent_size
		assert len(self.frequent) <= routing_cache.size - self.recent_size

	def try_evict_frequent(self):
		if len(self.frequent) > routing_cache.size - self.recent_size:
			el = self.frequent.pop()
			del self.cache[el]
			self.ghost_frequent.insert(0, el)
			if len(self.ghost_frequent) > routing_cache.size / 2:
				self.ghost_frequent.pop()

	def hit_recent(self, n_addr):
		if n_addr in self.recent:
			self.recent.remove(n_addr)
			self.frequent.insert(0, n_addr)
			self.try_evict_frequent()
			return True
		if n_addr in self.ghost_recent:
			self.ghost_recent.remove(n_addr)
			if routing_cache.size - self.recent_size > routing_cache.min_size:
				self.recent_size += 1
				self.try_evict_frequent()
		return False

	def try_evict_recent(self):
		if len(self.recent) > self.recent_size:
			el = self.recent.pop()
			del self.cache[el]
			self.ghost_recent.insert(0, el)
			if len(self.ghost_recent) > routing_cache.size / 2:
				self.ghost_recent.pop()

	def hit_frequent(self, n_addr):
		if n_addr in self.frequent:
			self.frequent.remove(n_addr)
			self.frequent.insert(0, n_addr)
			return True
		if n_addr in self.ghost_frequent:
			self.ghost_frequent.remove(n_addr)
			if self.recent_size > routing_cache.min_size:
				self.recent_size -= 1
				self.try_evict_recent()
		return False

	def dir_hit(self, n_addr):
		if not self.hit_recent(n_addr):
			self.hit_frequent(n_addr)

	def dir_add(self, n_addr):
		if not self.hit_recent(n_addr) and not self.hit_frequent(n_addr):
			self.recent.insert(0, n_addr)
			self.try_evict_recent()

	def direct_get(self, n_addr):
		if n_addr in self.cache:
			return self.cache[n_addr]
		return None

	def get(self, n_addr):
		self.assert_sizes
		self.dir_hit(n_addr)
		self.assert_sizes
		self.direct_get(n_addr)

	def add(self, n_addr, dl_addr):
		self.assert_sizes()
		self.dir_add(n_addr)
		self.assert_sizes()
		self.cache[n_addr] = dl_addr

