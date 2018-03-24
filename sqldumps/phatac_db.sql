-- phpMyAdmin SQL Dump
-- version 4.6.4
-- https://www.phpmyadmin.net/
--
-- Host: 127.0.0.1
-- Generation Time: Apr 27, 2017 at 01:43 AM
-- Server version: 10.1.22-MariaDB
-- PHP Version: 5.6.25

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `phatac`
--

-- --------------------------------------------------------

--
-- Table structure for table `accounts`
--

CREATE TABLE `accounts` (
  `id` int(11) NOT NULL,
  `username` varchar(40) NOT NULL,
  `password` varchar(129) NOT NULL,
  `password_salt` varchar(17) NOT NULL,
  `date_created` int(11) NOT NULL COMMENT 'unix timestamp',
  `access` int(11) NOT NULL COMMENT '0=lowest 10=highest',
  `created_ip_address` varchar(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `characters`
--

CREATE TABLE `characters` (
  `account_id` int(11) NOT NULL,
  `weenie_id` int(11) NOT NULL,
  `name` varchar(64) NOT NULL,
  `date_created` int(11) NOT NULL,
  `instance_ts` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `globals`
--

CREATE TABLE `globals` (
  `id` int(11) NOT NULL,
  `data` longblob NOT NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `teletowns`
--

CREATE TABLE `teletowns` (
  `ID` int(11) NOT NULL,
  `Description` varchar(50) NOT NULL,
  `Command` varchar(50) NOT NULL,
  `Landblock` varchar(50) NOT NULL,
  `Position_X` varchar(50) NOT NULL,
  `Position_Y` varchar(50) NOT NULL,
  `Position_Z` varchar(50) NOT NULL,
  `Orientation_W` varchar(50) NOT NULL,
  `Orientation_X` varchar(50) NOT NULL,
  `Orientation_Y` varchar(50) NOT NULL,
  `Orientation_Z` varchar(50) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `weenies`
--

CREATE TABLE `weenies` (
  `id` int(11) NOT NULL,
  `top_level_object_id` int(11) NOT NULL,
  `block_id` int(11) NOT NULL,
  `data` mediumblob NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `accounts`
--
ALTER TABLE `accounts`
  ADD PRIMARY KEY (`id`),
  ADD UNIQUE KEY `username` (`username`);

--
-- Indexes for table `characters`
--
ALTER TABLE `characters`
  ADD UNIQUE KEY `weenie_id` (`weenie_id`),
  ADD KEY `account_id` (`account_id`);

--
-- Indexes for table `globals`
--
ALTER TABLE `globals`
  ADD PRIMARY KEY (`id`);

--
-- Indexes for table `teletowns`
--
ALTER TABLE `teletowns`
  ADD PRIMARY KEY (`ID`);

--
-- Indexes for table `weenies`
--
ALTER TABLE `weenies`
  ADD PRIMARY KEY (`id`),
  ADD KEY `top_level_object_id` (`top_level_object_id`),
  ADD KEY `block_id` (`block_id`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `accounts`
--
ALTER TABLE `accounts`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=347;
--
-- AUTO_INCREMENT for table `globals`
--
ALTER TABLE `globals`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;
--
-- AUTO_INCREMENT for table `teletowns`
--
ALTER TABLE `teletowns`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=52;
--
-- AUTO_INCREMENT for table `weenies`
--
ALTER TABLE `weenies`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1342177499;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
