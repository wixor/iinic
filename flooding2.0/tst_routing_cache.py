import unittest
from routing_cache import routing_cache

class tst_routing_cache(unittest.TestCase):
	count = 20

	def test_recent_add(self):
		c = routing_cache()
		for i in range(tst_routing_cache.count):
			c.add(i, i)
		for i in range(tst_routing_cache.count - routing_cache.size / 2):
			self.assertEqual(c.direct_get(i), None)
		for i in range(tst_routing_cache.count - routing_cache.size / 2 + 1, tst_routing_cache.count):
			self.assertEqual(c.direct_get(i), i)

	def test_frequent_add(self):
		c = routing_cache()
		for i in range(routing_cache.size / 2):
			c.add(i, i)
		for i in range(routing_cache.size / 2):
			c.add(i, i)
		for i in range(tst_routing_cache.count):
			c.add(i, i)
		for i in range(routing_cache.size / 2, tst_routing_cache.count - routing_cache.size / 2):
			self.assertEqual(c.direct_get(i), None)
		for i in range(routing_cache.size / 2):
			self.assertEqual(c.direct_get(i), i)
		for i in range(tst_routing_cache.count - routing_cache.size / 2 + 1, tst_routing_cache.count):
			self.assertEqual(c.direct_get(i), i)

	def test_recent_ghost(self):
		c = routing_cache()
		for i in range(routing_cache.size):
			c.add(i, i)
		for i in range(tst_routing_cache.count):
			c.add(i, i)
		for i in range(tst_routing_cache.count - routing_cache.size / 2 - 1):
			self.assertEqual(c.direct_get(i), None)
		for i in range(tst_routing_cache.count - routing_cache.size / 2, tst_routing_cache.count):
			self.assertEqual(c.direct_get(i), i)

	def test_frequent_ghost(self):
		c = routing_cache()
		for i in range(routing_cache.size):
			c.add(i, i)
			c.add(i, i)
		for i in range(routing_cache.size):
			c.add(i, i)
			c.add(i, i)
		for i in range(tst_routing_cache.count, tst_routing_cache.count * 2):
			c.add(i, i)
		for i in range(routing_cache.min_size, routing_cache.size):
			self.assertEqual(c.direct_get(i), i)
		for i in range(routing_cache.size, tst_routing_cache.count * 2 - 1):
			self.assertEqual(c.direct_get(i), None)
		self.assertEqual(c.direct_get(0), None)
		self.assertEqual(c.direct_get(tst_routing_cache.count * 2 - 1), tst_routing_cache.count * 2 - 1)

	# ...

if __name__ == "__main__":
	unittest.main()
